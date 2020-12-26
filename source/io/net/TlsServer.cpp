/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "net/TlsServer.h"

#include "Convert.h"
#include "net/TcpServer.h"
#include "detail/ConstexprString.h"
#include "detail/TlsContext.h"
#include "detail/OpenSslContext.h"

#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/bn.h>

#include <iostream>
#include <memory>
#include <cstdio>

namespace tarm {
namespace io {
namespace net {

class TlsServer::Impl {
public:
    Impl(EventLoop& loop, const fs::Path& certificate_path, const fs::Path& private_key_path, TlsVersionRange version_range, TlsServer& parent);
    ~Impl();

    Error listen(const Endpoint endpoint,
                 const NewConnectionCallback& new_connection_callback,
                 const DataReceivedCallback& data_receive_callback,
                 const CloseConnectionCallback& close_connection_callback,
                 int backlog_size);

    void shutdown(const ShutdownServerCallback& shutdown_callback);
    void close(const CloseServerCallback& close_callback);

    std::size_t connected_clients_count() const;

    bool certificate_and_key_match();

    TlsVersionRange version_range() const;

    bool schedule_removal();

protected:
    const SSL_METHOD* ssl_method();

    // callbacks
    void on_new_connection(TcpConnectedClient& tcp_client, const Error& tcp_error);
    void on_data_receive(TcpConnectedClient& tcp_client, const DataChunk&, const Error& tcp_error);
    void on_connection_close(TcpConnectedClient& tcp_client, const Error& tcp_error);

private:
    using X509Ptr = std::unique_ptr<::X509, decltype(&::X509_free)>;
    using EvpPkeyPtr = std::unique_ptr<::EVP_PKEY, decltype(&::EVP_PKEY_free)>;

    TlsServer* m_parent;
    EventLoop* m_loop;
    TcpServer* m_tcp_server;

    fs::Path m_certificate_path;
    fs::Path m_private_key_path;

    X509Ptr m_certificate;
    EvpPkeyPtr m_private_key;
    TlsVersionRange m_version_range;

    detail::OpenSslContext<TlsServer, TlsServer::Impl> m_openssl_context;

    NewConnectionCallback m_new_connection_callback = nullptr;
    DataReceivedCallback m_data_receive_callback = nullptr;
    CloseConnectionCallback m_close_connection_callback = nullptr;
};

TlsServer::Impl::Impl(EventLoop& loop,
                         const fs::Path& certificate_path,
                         const fs::Path& private_key_path,
                         TlsVersionRange version_range,
                         TlsServer& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_tcp_server(new TcpServer(loop)),
    m_certificate_path(certificate_path),
    m_private_key_path(private_key_path),
    m_certificate(nullptr, ::X509_free),
    m_private_key(nullptr, ::EVP_PKEY_free),
    m_version_range(version_range),
    m_openssl_context(loop, parent) {
}

TlsServer::Impl::~Impl() {
}

bool TlsServer::Impl::schedule_removal() {
    LOG_TRACE(m_loop, m_parent, "");

    if (m_parent->is_removal_scheduled()) {
        LOG_TRACE(m_loop, m_parent, "is_removal_scheduled: true");
        return true;
    }

    if (m_tcp_server->is_open()) {
        m_tcp_server->close([this](TcpServer& server, const Error& error) {
            if (error.code() != StatusCode::NOT_CONNECTED) {
                LOG_ERROR(this->m_loop, error);
            }

            this->m_parent->schedule_removal();
            server.schedule_removal();
        });

        m_parent->set_removal_scheduled();
        return false;
    } else {
        m_tcp_server->schedule_removal();
        return true;
    }
}

const SSL_METHOD* TlsServer::Impl::ssl_method() {
    return SSLv23_server_method(); // This call includes also TLS versions
}

TlsVersionRange TlsServer::Impl::version_range() const {
    return m_version_range;
}

void TlsServer::Impl::on_new_connection(TcpConnectedClient& tcp_client, const Error& tcp_error) {
    detail::TlsContext context {
        m_certificate.get(),
        m_private_key.get(),
        m_openssl_context.ssl_ctx(),
        m_version_range
    };

    // Can not use unique_ptr here because TlsConnectedClient has proteted destructor and
    // TlsServer is a friend of TlsConnectedClient, but we can not transfer that friendhsip to unique_ptr.
    auto tls_client = new TlsConnectedClient(*m_loop, *m_parent, m_new_connection_callback, tcp_client, &context);

    if (tcp_error) {
        if (m_new_connection_callback) {
            m_new_connection_callback(*tls_client, tcp_error);
        }
        delete tls_client;
        return;
    }

    Error tls_init_error = tls_client->init_ssl();
    if (tls_init_error) {
        if (m_new_connection_callback) {
            m_new_connection_callback(*tls_client, tls_init_error);
        }
        tcp_client.set_user_data(nullptr); // to not process deletion in on_connection_close
        tcp_client.close();
        delete tls_client;
    } else {
        tls_client->set_data_receive_callback(m_data_receive_callback);
    }
}

void TlsServer::Impl::on_data_receive(TcpConnectedClient& tcp_client, const DataChunk& chunk, const Error& tcp_error) {
    auto& tls_client = *reinterpret_cast<TlsConnectedClient*>(tcp_client.user_data());
    tls_client.on_data_receive(chunk.buf.get(), chunk.size, tcp_error);
}

void TlsServer::Impl::on_connection_close(TcpConnectedClient& tcp_client, const Error& tcp_error) {
    LOG_TRACE(this->m_loop, "Removing TLS client");

    if (tcp_client.user_data()) {
        auto& tls_client = *reinterpret_cast<TlsConnectedClient*>(tcp_client.user_data());
        if (m_close_connection_callback) {
            m_close_connection_callback(tls_client, tcp_error);
        }

        delete &tls_client;
    }
}

Error TlsServer::Impl::listen(const Endpoint endpoint,
                                 const NewConnectionCallback& new_connection_callback,
                                 const DataReceivedCallback& data_receive_callback,
                                 const CloseConnectionCallback& close_connection_callback,
                                 int backlog_size) {
    m_new_connection_callback = new_connection_callback;
    m_data_receive_callback = data_receive_callback;
    m_close_connection_callback = close_connection_callback;

    using FilePtr = std::unique_ptr<FILE, decltype(&std::fclose)>;

    FilePtr certificate_file(std::fopen(m_certificate_path.string().c_str(), "r"), &std::fclose);
    if (certificate_file == nullptr) {
        return Error(StatusCode::TLS_CERTIFICATE_FILE_NOT_EXIST);
    }

    m_certificate.reset(PEM_read_X509(certificate_file.get(), nullptr, nullptr, nullptr));
    if (m_certificate == nullptr) {
        return Error(StatusCode::TLS_CERTIFICATE_INVALID);
    }

    FilePtr private_key_file(std::fopen(m_private_key_path.string().c_str(), "r"), &std::fclose);
    if (private_key_file == nullptr) {
        return Error(StatusCode::TLS_PRIVATE_KEY_FILE_NOT_EXIST);
    }

    m_private_key.reset(PEM_read_PrivateKey(private_key_file.get(), nullptr, nullptr, nullptr));
    if (m_private_key == nullptr) {
        return Error(StatusCode::TLS_PRIVATE_KEY_INVALID);
    }

    if (!certificate_and_key_match()) {
        return Error(StatusCode::TLS_PRIVATE_KEY_AND_CERTIFICATE_NOT_MATCH);
    }

    const auto& context_init_error = m_openssl_context.init_ssl_context(ssl_method());
    if (context_init_error) {
        return context_init_error;
    }

    const auto& version_error = m_openssl_context.set_tls_version(std::get<0>(m_version_range), std::get<1>(m_version_range));
    if (version_error) {
        return version_error;
    }

    const auto& certificate_error = m_openssl_context.ssl_init_certificate_and_key(m_certificate.get(), m_private_key.get());
    if (certificate_error) {
        return certificate_error;
    }

    using namespace std::placeholders;
    return m_tcp_server->listen(endpoint,
                                std::bind(&TlsServer::Impl::on_new_connection, this, _1, _2),
                                std::bind(&TlsServer::Impl::on_data_receive, this, _1, _2, _3),
                                std::bind(&TlsServer::Impl::on_connection_close, this, _1, _2));
}

void TlsServer::Impl::shutdown(const ShutdownServerCallback& shutdown_callback) {
    if (shutdown_callback) {
        m_tcp_server->shutdown([this, shutdown_callback](TcpServer&, const Error& error) {
            shutdown_callback(*m_parent, error);
        });
    } else {
        m_tcp_server->shutdown();
    }
}

void TlsServer::Impl::close(const CloseServerCallback& close_callback) {
    if (close_callback) {
        m_tcp_server->shutdown([this, close_callback](TcpServer&, const Error& error) {
            close_callback(*m_parent, error);
        });
    } else {
        m_tcp_server->close();
    }
}

std::size_t TlsServer::Impl::connected_clients_count() const {
    return m_tcp_server->connected_clients_count();
}

namespace {

//int ssl_key_type(::EVP_PKEY* pkey) {
//    assert(pkey);
//    return pkey ? EVP_PKEY_type(pkey->type) : NID_undef;
//}

} // namespace

bool TlsServer::Impl::certificate_and_key_match() {
    assert(m_certificate);
    assert(m_private_key);

    return X509_verify(m_certificate.get(), m_private_key.get()) != 0;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////



TlsServer::TlsServer(EventLoop& loop, const fs::Path& certificate_path, const fs::Path& private_key_path, TlsVersionRange version_range) :
    Removable(loop),
    m_impl(new Impl(loop, certificate_path, private_key_path, version_range, *this)) {
}

TlsServer::~TlsServer() {
}

Error TlsServer::listen(const Endpoint endpoint,
                           const NewConnectionCallback& new_connection_callback,
                           const DataReceivedCallback& data_receive_callback,
                           const CloseConnectionCallback& close_connection_callback,
                           int backlog_size) {
    return m_impl->listen(endpoint, new_connection_callback, data_receive_callback, close_connection_callback, backlog_size);
}

Error TlsServer::listen(const Endpoint endpoint,
                           const DataReceivedCallback& data_receive_callback,
                           int backlog_size) {
    return m_impl->listen(endpoint, nullptr, data_receive_callback, nullptr, backlog_size);
}

Error TlsServer::listen(const Endpoint endpoint,
                           const NewConnectionCallback& new_connection_callback,
                           const DataReceivedCallback& data_receive_callback,
                           int backlog_size) {
    return m_impl->listen(endpoint, new_connection_callback, data_receive_callback, nullptr, backlog_size);
}

void TlsServer::shutdown(const CloseServerCallback& shutdown_callback) {
    return m_impl->shutdown(shutdown_callback);
}

void TlsServer::close(const CloseServerCallback& close_callback) {
    return m_impl->close(close_callback);
}

std::size_t TlsServer::connected_clients_count() const {
    return m_impl->connected_clients_count();
}

TlsVersionRange TlsServer::version_range() const {
    return m_impl->version_range();
}

void TlsServer::schedule_removal() {
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

} // namespace net
} // namespace io
} // namespace tarm

/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "TlsTcpServer.h"

#include "Convert.h"
#include "TcpServer.h"
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

class TlsTcpServer::Impl {
public:
    Impl(EventLoop& loop, const Path& certificate_path, const Path& private_key_path, TlsVersionRange version_range, TlsTcpServer& parent);
    ~Impl();

    Error listen(const Endpoint endpoint,
                 NewConnectionCallback new_connection_callback,
                 DataReceivedCallback data_receive_callback,
                 CloseConnectionCallback close_connection_callback,
                 int backlog_size);

    void shutdown(ShutdownServerCallback shutdown_callback);
    void close(CloseServerCallback close_callback);

    std::size_t connected_clients_count() const;

    bool certificate_and_key_match();

    TlsVersionRange version_range() const;

    bool schedule_removal();

protected:
    const SSL_METHOD* ssl_method();

    // callbacks
    void on_new_connection(TcpConnectedClient& tcp_client, const Error& error);
    void on_data_receive(TcpConnectedClient& tcp_client, const DataChunk&, const Error&);
    void on_close(TcpConnectedClient& tcp_client, const Error& error);

private:
    using X509Ptr = std::unique_ptr<::X509, decltype(&::X509_free)>;
    using EvpPkeyPtr = std::unique_ptr<::EVP_PKEY, decltype(&::EVP_PKEY_free)>;

    TlsTcpServer* m_parent;
    EventLoop* m_loop;
    TcpServer* m_tcp_server;

    Path m_certificate_path;
    Path m_private_key_path;

    X509Ptr m_certificate;
    EvpPkeyPtr m_private_key;
    TlsVersionRange m_version_range;

    detail::OpenSslContext<TlsTcpServer, TlsTcpServer::Impl> m_openssl_context;

    NewConnectionCallback m_new_connection_callback = nullptr;
    DataReceivedCallback m_data_receive_callback = nullptr;
    CloseConnectionCallback m_close_connection_callback = nullptr;
};

TlsTcpServer::Impl::Impl(EventLoop& loop,
                         const Path& certificate_path,
                         const Path& private_key_path,
                         TlsVersionRange version_range,
                         TlsTcpServer& parent) :
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

TlsTcpServer::Impl::~Impl() {
}

bool TlsTcpServer::Impl::schedule_removal() {
    IO_LOG(m_loop, TRACE, m_parent, "");

    if (m_parent->is_removal_scheduled()) {
        IO_LOG(m_loop, TRACE, m_parent, "is_removal_scheduled: true");
        return true;
    }

    m_tcp_server->close([this](TcpServer& server, const Error& error) {
        if (error.code() != StatusCode::NOT_CONNECTED) {
            IO_LOG(this->m_loop, ERROR, error);
        }

        this->m_parent->schedule_removal();
        server.schedule_removal();
    });

    m_parent->set_removal_scheduled();
    return false;
}

const SSL_METHOD* TlsTcpServer::Impl::ssl_method() {
    return SSLv23_server_method(); // This call includes also TLS versions
}

TlsVersionRange TlsTcpServer::Impl::version_range() const {
    return m_version_range;
}

void TlsTcpServer::Impl::on_new_connection(TcpConnectedClient& tcp_client, const Error& error) {
    if (error) {
        //m_new_connection_callback(.......);
        // TODO: error handling here
        return;
    }

    detail::TlsContext context {
        m_certificate.get(),
        m_private_key.get(),
        m_openssl_context.ssl_ctx(),
        m_version_range
    };

    TlsTcpConnectedClient* tls_client =
        new TlsTcpConnectedClient(*m_loop, *m_parent, m_new_connection_callback, tcp_client, &context);

    // TODO: error
    Error tls_init_error = tls_client->init_ssl();
    if (!tls_init_error) {
        tls_client->set_data_receive_callback(m_data_receive_callback);
    }
}

void TlsTcpServer::Impl::on_data_receive(TcpConnectedClient& tcp_client, const DataChunk& chunk, const Error& error) {
    // TODO: handle error
    auto& tls_client = *reinterpret_cast<TlsTcpConnectedClient*>(tcp_client.user_data());
    tls_client.on_data_receive(chunk.buf.get(), chunk.size);
}

void TlsTcpServer::Impl::on_close(TcpConnectedClient& tcp_client, const Error& error) {
    IO_LOG(this->m_loop, TRACE, "Removing TLS client");

    auto& tls_client = *reinterpret_cast<TlsTcpConnectedClient*>(tcp_client.user_data());
    if (m_close_connection_callback) {
        m_close_connection_callback(tls_client, error);
    }

    delete &tls_client;
}

Error TlsTcpServer::Impl::listen(const Endpoint endpoint,
                                 NewConnectionCallback new_connection_callback,
                                 DataReceivedCallback data_receive_callback,
                                 CloseConnectionCallback close_connection_callback,
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
                                std::bind(&TlsTcpServer::Impl::on_new_connection, this, _1, _2),
                                std::bind(&TlsTcpServer::Impl::on_data_receive, this, _1, _2, _3),
                                std::bind(&TlsTcpServer::Impl::on_close, this, _1, _2));
}

void TlsTcpServer::Impl::shutdown(ShutdownServerCallback shutdown_callback) {
    if (shutdown_callback) {
        m_tcp_server->shutdown([this, shutdown_callback](TcpServer&, const Error& error) {
            shutdown_callback(*m_parent, error);
        });
    } else {
        m_tcp_server->shutdown();
    }
}

void TlsTcpServer::Impl::close(CloseServerCallback close_callback) {
    if (close_callback) {
        m_tcp_server->shutdown([this, close_callback](TcpServer&, const Error& error) {
            close_callback(*m_parent, error);
        });
    } else {
        m_tcp_server->close();
    }
}

std::size_t TlsTcpServer::Impl::connected_clients_count() const {
    return m_tcp_server->connected_clients_count();
}

namespace {

//int ssl_key_type(::EVP_PKEY* pkey) {
//    assert(pkey);
//    return pkey ? EVP_PKEY_type(pkey->type) : NID_undef;
//}

} // namespace

bool TlsTcpServer::Impl::certificate_and_key_match() {
    assert(m_certificate);
    assert(m_private_key);

    return X509_verify(m_certificate.get(), m_private_key.get()) != 0;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////



TlsTcpServer::TlsTcpServer(EventLoop& loop, const Path& certificate_path, const Path& private_key_path, TlsVersionRange version_range) :
    Removable(loop),
    m_impl(new Impl(loop, certificate_path, private_key_path, version_range, *this)) {
}

TlsTcpServer::~TlsTcpServer() {
}

Error TlsTcpServer::listen(const Endpoint endpoint,
                           NewConnectionCallback new_connection_callback,
                           DataReceivedCallback data_receive_callback,
                           CloseConnectionCallback close_connection_callback,
                           int backlog_size) {
    return m_impl->listen(endpoint, new_connection_callback, data_receive_callback, close_connection_callback, backlog_size);
}

Error TlsTcpServer::listen(const Endpoint endpoint,
                           DataReceivedCallback data_receive_callback,
                           int backlog_size) {
    return m_impl->listen(endpoint, nullptr, data_receive_callback, nullptr, backlog_size);
}

Error TlsTcpServer::listen(const Endpoint endpoint,
                           NewConnectionCallback new_connection_callback,
                           DataReceivedCallback data_receive_callback,
                           int backlog_size) {
    return m_impl->listen(endpoint, new_connection_callback, data_receive_callback, nullptr, backlog_size);
}

void TlsTcpServer::shutdown(CloseServerCallback shutdown_callback) {
    return m_impl->shutdown(shutdown_callback);
}

void TlsTcpServer::close(CloseServerCallback close_callback) {
    return m_impl->close(close_callback);
}

std::size_t TlsTcpServer::connected_clients_count() const {
    return m_impl->connected_clients_count();
}

TlsVersionRange TlsTcpServer::version_range() const {
    return m_impl->version_range();
}

void TlsTcpServer::schedule_removal() {
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

} // namespace io
} // namespace tarm

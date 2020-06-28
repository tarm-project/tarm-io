/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "DtlsServer.h"

#include "Convert.h"
#include "UdpServer.h"
#include "UdpPeer.h"
#include "detail/ConstexprString.h"
#include "detail/DtlsContext.h"
#include "detail/OpenSslContext.h"

#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/bn.h>

#include <cstdio>
#include <iostream>
#include <memory>
#include <unordered_set>

namespace tarm {
namespace io {

class DtlsServer::Impl {
public:
    Impl(EventLoop& loop, const fs::Path& certificate_path, const fs::Path& private_key_path, DtlsVersionRange version_range, DtlsServer& parent);
    ~Impl();

    Error listen(const Endpoint& endpoint,
                 const NewConnectionCallback& new_connection_callback,
                 const DataReceivedCallback& data_receive_callback,
                 std::size_t timeout_ms,
                 const CloseConnectionCallback& close_callback);

    void close(const CloseServerCallback& callback);

    std::size_t connected_clients_count() const;

    bool certificate_and_key_match();

    void remove_client(DtlsConnectedClient& client);

    bool schedule_removal();

protected:
    const SSL_METHOD* ssl_method();

    // callbacks
    void on_new_peer(UdpPeer& udp_client, const Error& error);
    void on_data_receive(UdpPeer& udp_client, const DataChunk& data, const Error& error);
    void on_timeout(UdpPeer& udp_peer, const Error& error);

private:
    using X509Ptr = std::unique_ptr<::X509, decltype(&::X509_free)>;
    using EvpPkeyPtr = std::unique_ptr<::EVP_PKEY, decltype(&::EVP_PKEY_free)>;

    DtlsServer* m_parent;
    EventLoop* m_loop;
    UdpServer* m_udp_server;

    fs::Path m_certificate_path;
    fs::Path m_private_key_path;

    X509Ptr m_certificate;
    EvpPkeyPtr m_private_key;
    DtlsVersionRange m_version_range;

    detail::OpenSslContext<DtlsServer, DtlsServer::Impl> m_openssl_context;

    NewConnectionCallback m_new_connection_callback = nullptr;
    DataReceivedCallback m_data_receive_callback = nullptr;
    CloseConnectionCallback m_connection_close_callback = nullptr;

    CloseServerCallback m_close_server_callback = nullptr;

    std::unordered_set<DtlsConnectedClient*> m_clients;
};

DtlsServer::Impl::Impl(EventLoop& loop, const fs::Path& certificate_path, const fs::Path& private_key_path, DtlsVersionRange version_range, DtlsServer& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_udp_server(new UdpServer(loop)),
    m_certificate_path(certificate_path),
    m_private_key_path(private_key_path),
    m_certificate(nullptr, ::X509_free),
    m_private_key(nullptr, ::EVP_PKEY_free),
    m_version_range(version_range),
    m_openssl_context(loop, parent) {
}

DtlsServer::Impl::~Impl() {
    m_udp_server->schedule_removal();
}

void DtlsServer::Impl::on_new_peer(UdpPeer& udp_client, const Error& error) {
    if (error) {
        if (m_new_connection_callback) {
            DtlsConnectedClient temporary_client(*m_loop, *m_parent, udp_client);
            m_new_connection_callback(temporary_client, error);
        }
        return;
    }

    detail::DtlsContext context {
        m_certificate.get(),
        m_private_key.get(),
        m_openssl_context.ssl_ctx(),
        m_version_range
    };

    DtlsConnectedClient* dtls_client =
        new DtlsConnectedClient(*m_loop, *m_parent, m_new_connection_callback, m_connection_close_callback, udp_client, &context);
    dtls_client->set_user_data(&udp_client);
    m_clients.insert(dtls_client);
    udp_client.set_on_schedule_removal(
        [=](const Removable&) {
            delete dtls_client;
        }
    );

    Error init_error = dtls_client->init_ssl();
    if (!init_error) {
        dtls_client->set_data_receive_callback(m_data_receive_callback);
    } else {
        if (m_new_connection_callback) {
            m_new_connection_callback(*dtls_client, init_error);
        }
    }
}

void DtlsServer::Impl::on_data_receive(UdpPeer& udp_client, const DataChunk& data, const Error& error) {
    auto& dtls_client = *reinterpret_cast<DtlsConnectedClient*>(udp_client.user_data());
    dtls_client.on_data_receive(data.buf.get(), data.size);
}

void DtlsServer::Impl::on_timeout(UdpPeer& udp_peer, const Error& error) {
    auto& dtls_client = *reinterpret_cast<DtlsConnectedClient*>(udp_peer.user_data());

    IO_LOG(this->m_loop, TRACE, &dtls_client, "Removing DTLS client due to timeout");

    //udp_peer.set_on_schedule_removal(nullptr);
    dtls_client.close();

    /*
    remove_client(dtls_client);

    if (m_connection_close_callback) {
        m_connection_close_callback(dtls_client, error);
    }

    delete &dtls_client;
    */
}

Error DtlsServer::Impl::listen(const Endpoint& endpoint,
                               const NewConnectionCallback& new_connection_callback,
                               const DataReceivedCallback& data_receive_callback,
                               std::size_t timeout_ms,
                               const CloseConnectionCallback& close_callback) {
    if (endpoint.type() == Endpoint::UNDEFINED) {
        return Error(StatusCode::INVALID_ARGUMENT);
    }

    m_new_connection_callback = new_connection_callback;
    m_data_receive_callback = data_receive_callback;
    m_connection_close_callback = close_callback;

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

    const auto& version_error = m_openssl_context.set_dtls_version(std::get<0>(m_version_range), std::get<1>(m_version_range));
    if (version_error) {
        return version_error;
    }

    const auto& certificate_error = m_openssl_context.ssl_init_certificate_and_key(m_certificate.get(), m_private_key.get());
    if (certificate_error) {
        return certificate_error;
    }

    using namespace std::placeholders;
    return m_udp_server->start_receive(endpoint,
                                       std::bind(&DtlsServer::Impl::on_new_peer, this, _1, _2),
                                       std::bind(&DtlsServer::Impl::on_data_receive, this, _1, _2, _3),
                                       timeout_ms,
                                       std::bind(&DtlsServer::Impl::on_timeout, this, _1, _2));
}

void DtlsServer::Impl::close(const CloseServerCallback& callback) {
    std::unordered_set<DtlsConnectedClient*> clients;
    clients.swap(m_clients);

    for (auto& c : clients) {
        c->close();
    }

    m_close_server_callback = callback;

    return m_udp_server->close(
        [this](UdpServer&, const Error& error) {
            if (m_close_server_callback) {
                m_close_server_callback(*m_parent, error);
            }
        }
    );
}

std::size_t DtlsServer::Impl::connected_clients_count() const {
    return m_clients.size();
}

bool DtlsServer::Impl::certificate_and_key_match() {
    assert(m_certificate);
    assert(m_private_key);

    return X509_verify(m_certificate.get(), m_private_key.get()) != 0;
}

const SSL_METHOD* DtlsServer::Impl::ssl_method() {
// OpenSSL before version 1.0.2 had no generic method for DTLS and only supported DTLS 1.0
#if OPENSSL_VERSION_NUMBER < 0x1000200fL
    return DTLSv1_server_method();
#else
    return DTLS_server_method();
#endif
}

void DtlsServer::Impl::remove_client(DtlsConnectedClient& client) {
    auto it = m_clients.find(&client);
    if (it != m_clients.end()) {
        m_clients.erase(it);
    }
}

bool DtlsServer::Impl::schedule_removal() {
    if (m_clients.empty()) {
        return true;
    }

    close([](DtlsServer& server, const Error&) {
        server.schedule_removal();
    });

    return false;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

DtlsServer::DtlsServer(EventLoop& loop, const fs::Path& certificate_path, const fs::Path& private_key_path, DtlsVersionRange version_range) :
    Removable(loop),
    m_impl(new Impl(loop, certificate_path, private_key_path, version_range, *this)) {
}

DtlsServer::~DtlsServer() {
}

Error DtlsServer::listen(const Endpoint& endpoint,
                         const DataReceivedCallback& data_receive_callback) {
    return m_impl->listen(endpoint, nullptr, data_receive_callback, DEFAULT_TIMEOUT_MS, nullptr);
}

Error DtlsServer::listen(const Endpoint& endpoint,
                         const NewConnectionCallback& new_connection_callback,
                         const DataReceivedCallback& data_receive_callback) {
    return m_impl->listen(endpoint, new_connection_callback, data_receive_callback, DEFAULT_TIMEOUT_MS, nullptr);
}

Error DtlsServer::listen(const Endpoint& endpoint,
                         const NewConnectionCallback& new_connection_callback,
                         const DataReceivedCallback& data_receive_callback,
                         std::size_t timeout_ms,
                         const CloseConnectionCallback& close_callback) {
    return m_impl->listen(endpoint, new_connection_callback, data_receive_callback, timeout_ms, close_callback);
}

void DtlsServer::close(const CloseServerCallback& callback) {
    return m_impl->close(callback);
}

std::size_t DtlsServer::connected_clients_count() const {
    return m_impl->connected_clients_count();
}

void DtlsServer::remove_client(DtlsConnectedClient& client) {
    return m_impl->remove_client(client);
}

void DtlsServer::schedule_removal() {
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

} // namespace io
} // namespace tarm


#include "TlsTcpServer.h"

#include "detail/TlsContext.h"

#include "Common.h"
#include "TcpServer.h"

#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/bn.h>

#include <iostream>
#include <memory>
#include <cstdio>

namespace io {

class TlsTcpServer::Impl {
public:
    Impl(EventLoop& loop, const Path& certificate_path, const Path& private_key_path, TlsVersionRange version_range, TlsTcpServer& parent);
    ~Impl();

    Error listen(const std::string& ip_addr_str,
                 std::uint16_t port,
                 NewConnectionCallback new_connection_callback,
                 DataReceivedCallback data_receive_callback,
                 CloseConnectionCallback close_connection_callback,
                 int backlog_size);

    void shutdown();
    void close();

    std::size_t connected_clients_count() const;

    bool certificate_and_key_match();

    TlsVersionRange version_range() const;

protected: // callbacks
    void on_new_connection(TcpConnectedClient& tcp_client, const io::Error& error);
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
    m_version_range(version_range) {
}

TlsTcpServer::Impl::~Impl() {
    m_tcp_server->schedule_removal();
}

TlsVersionRange TlsTcpServer::Impl::version_range() const {
    return m_version_range;
}

void TlsTcpServer::Impl::on_new_connection(TcpConnectedClient& tcp_client, const io::Error& error) {
    if (error) {
        //m_new_connection_callback(.......);
        // TODO: error handling here
        return;
    }

    detail::TlsContext context {
        m_certificate.get(),
        m_private_key.get(),
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

Error TlsTcpServer::Impl::listen(const std::string& ip_addr_str,
                                 std::uint16_t port,
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

    using namespace std::placeholders;
    return m_tcp_server->listen(ip_addr_str,
                                port,
                                std::bind(&TlsTcpServer::Impl::on_new_connection, this, _1, _2),
                                std::bind(&TlsTcpServer::Impl::on_data_receive, this, _1, _2, _3),
                                std::bind(&TlsTcpServer::Impl::on_close, this, _1, _2));
}

void TlsTcpServer::Impl::shutdown() {
    return m_tcp_server->shutdown();
}

void TlsTcpServer::Impl::close() {
    return m_tcp_server->close();
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

IO_DEFINE_DEFAULT_MOVE(TlsTcpServer);

TlsTcpServer::TlsTcpServer(EventLoop& loop, const Path& certificate_path, const Path& private_key_path, TlsVersionRange version_range) :
    Removable(loop),
    m_impl(new Impl(loop, certificate_path, private_key_path, version_range, *this)) {
}

TlsTcpServer::~TlsTcpServer() {
}

Error TlsTcpServer::listen(const std::string& ip_addr_str,
                           std::uint16_t port,
                           NewConnectionCallback new_connection_callback,
                           DataReceivedCallback data_receive_callback,
                           CloseConnectionCallback close_connection_callback,
                           int backlog_size) {
    return m_impl->listen(ip_addr_str, port, new_connection_callback, data_receive_callback, close_connection_callback, backlog_size);
}

Error TlsTcpServer::listen(const std::string& ip_addr_str,
                           std::uint16_t port,
                           DataReceivedCallback data_receive_callback,
                           int backlog_size) {
    return m_impl->listen(ip_addr_str, port, nullptr, data_receive_callback, nullptr, backlog_size);
}

Error TlsTcpServer::listen(const std::string& ip_addr_str,
                           std::uint16_t port,
                           NewConnectionCallback new_connection_callback,
                           DataReceivedCallback data_receive_callback,
                           int backlog_size) {
    return m_impl->listen(ip_addr_str, port, new_connection_callback, data_receive_callback, nullptr, backlog_size);
}

void TlsTcpServer::shutdown() {
    return m_impl->shutdown();
}

void TlsTcpServer::close() {
    return m_impl->close();
}

std::size_t TlsTcpServer::connected_clients_count() const {
    return m_impl->connected_clients_count();
}

TlsVersionRange TlsTcpServer::version_range() const {
    return m_impl->version_range();
}

} // namespace io

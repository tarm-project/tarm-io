#include "TlsTcpServer.h"

#include "TcpServer.h"

#include <openssl/pem.h>

#include <iostream>

namespace io {

class TlsTcpServer::Impl {
public:
    Impl(EventLoop& loop, const Path& certificate_path, const Path& private_key_path, TlsTcpServer& parent);
    ~Impl();

    Error bind(const std::string& ip_addr_str, std::uint16_t port);

    int listen(NewConnectionCallback new_connection_callback,
               DataReceivedCallback data_receive_callback,
               int backlog_size);

    void shutdown();
    void close();

    std::size_t connected_clients_count() const;

protected: // callbacks
    bool on_new_connection(TcpServer& server, TcpConnectedClient& tcp_client);
    void on_data_receive(TcpServer& server, TcpConnectedClient& tcp_client, const char* buf, std::size_t size);

private:
    TlsTcpServer* m_parent;
    EventLoop* m_loop;
    TcpServer* m_tcp_server;

    Path m_certificate_path;
    Path m_private_key_path;

    X509* m_certificate;
    EVP_PKEY* m_private_key;

    NewConnectionCallback m_new_connection_callback = nullptr;
    DataReceivedCallback m_data_receive_callback = nullptr;
};

TlsTcpServer::Impl::Impl(EventLoop& loop, const Path& certificate_path, const Path& private_key_path, TlsTcpServer& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_tcp_server(new TcpServer(loop)),
    m_certificate_path(certificate_path),
    m_private_key_path(private_key_path) {
}

TlsTcpServer::Impl::~Impl() {
    delete m_tcp_server; // TODO: schedule removal fro TCP server
}

Error TlsTcpServer::Impl::bind(const std::string& ip_addr_str, std::uint16_t port) {
    return m_tcp_server->bind(ip_addr_str, port);
}

bool TlsTcpServer::Impl::on_new_connection(TcpServer& server, TcpConnectedClient& tcp_client) {
    TlsTcpConnectedClient* tls_client = new TlsTcpConnectedClient(*m_loop, *m_parent, m_certificate, m_private_key, tcp_client);

    bool accept = true;
    if (m_new_connection_callback) {
        accept = m_new_connection_callback(*m_parent, *tls_client);
    }

    if (accept) {
        tls_client->set_data_receive_callback(m_data_receive_callback);
    } else {
        // TODO: delete
    }

    return accept;
}

void TlsTcpServer::Impl::on_data_receive(TcpServer& server, TcpConnectedClient& tcp_client, const char* buf, std::size_t size) {
    auto& tls_client = *reinterpret_cast<TlsTcpConnectedClient*>(tcp_client.user_data());
    tls_client.on_data_receive(buf, size);
}

int TlsTcpServer::Impl::listen(NewConnectionCallback new_connection_callback,
                               DataReceivedCallback data_receive_callback,
                               int backlog_size) {
    m_new_connection_callback = new_connection_callback;
    m_data_receive_callback = data_receive_callback;

    FILE* certificate_file = fopen(m_certificate_path.string().c_str(), "r");
    if (certificate_file == nullptr) {
        return -1; // TODO:
    }

    m_certificate = PEM_read_X509(certificate_file, nullptr, nullptr, nullptr);
    if (m_certificate == nullptr) {
        return -1; // TODO:
    }

    FILE* private_key_file = fopen(m_private_key_path.string().c_str(), "r");
    if (private_key_file == nullptr) {
        return -1; // TODO:
    }

    m_private_key = PEM_read_PrivateKey(private_key_file, nullptr, nullptr, nullptr);
    if (m_private_key == nullptr) {
        return -1; // TODO:
    }

    using namespace std::placeholders;
    return m_tcp_server->listen(std::bind(&TlsTcpServer::Impl::on_new_connection, this, _1, _2),
                                std::bind(&TlsTcpServer::Impl::on_data_receive, this, _1, _2, _3, _4));
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

///////////////////////////////////////// implementation ///////////////////////////////////////////

TlsTcpServer::TlsTcpServer(EventLoop& loop, const Path& certificate_path, const Path& private_key_path) :
    Disposable(loop),
    m_impl(new Impl(loop, certificate_path, private_key_path, *this)) {
}

TlsTcpServer::~TlsTcpServer() {
}

Error TlsTcpServer::bind(const std::string& ip_addr_str, std::uint16_t port) {
    return m_impl->bind(ip_addr_str, port);
}

int TlsTcpServer::listen(NewConnectionCallback new_connection_callback,
                         DataReceivedCallback data_receive_callback,
                         int backlog_size) {
    return m_impl->listen(new_connection_callback, data_receive_callback, backlog_size);
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

} // namespace io

#include "TlsTcpConnectedClient.h"

#include "TcpConnectedClient.h"

namespace io {

class TlsTcpConnectedClient::Impl {
public:
    Impl(EventLoop& loop, TlsTcpServer& tls_server, TcpConnectedClient& tcp_client, TlsTcpConnectedClient& parent);
    ~Impl();

    void set_data_receive_callback(DataReceiveCallback callback);
    void on_data_receive(const char* buf, std::size_t size);

private:
    TlsTcpConnectedClient* m_parent;
    EventLoop* m_loop;
    TcpConnectedClient* m_tcp_client;
    TlsTcpServer* m_tls_server;

    DataReceiveCallback m_data_receive_callback = nullptr;
};

TlsTcpConnectedClient::Impl::Impl(EventLoop& loop, TlsTcpServer& tls_server, TcpConnectedClient& tcp_client, TlsTcpConnectedClient& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_tcp_client(&tcp_client),
    m_tls_server(&tls_server) {
    m_tcp_client->set_user_data(&parent);
}

TlsTcpConnectedClient::Impl::~Impl() {
}

void TlsTcpConnectedClient::Impl::set_data_receive_callback(DataReceiveCallback callback) {
    m_data_receive_callback = callback;
}

void TlsTcpConnectedClient::Impl::on_data_receive(const char* buf, std::size_t size) {
    if (m_data_receive_callback) {
        m_data_receive_callback(*m_tls_server, *m_parent, buf, size);
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TlsTcpConnectedClient::TlsTcpConnectedClient(EventLoop& loop, TlsTcpServer& tls_server, TcpConnectedClient& tcp_client) :
    Disposable(loop),
    m_impl(new Impl(loop, tls_server, tcp_client, *this)) {
}

TlsTcpConnectedClient::~TlsTcpConnectedClient() {
}

void TlsTcpConnectedClient::set_data_receive_callback(DataReceiveCallback callback) {
    return m_impl->set_data_receive_callback(callback);
}

void TlsTcpConnectedClient::on_data_receive(const char* buf, std::size_t size) {
    return m_impl->on_data_receive(buf, size);
}

} // namespace io


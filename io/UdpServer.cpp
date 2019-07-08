#include "UdpServer.h"

namespace io {

class UdpServer::Impl : public uv_udp_t {
public:
    Impl(EventLoop& loop);

    int bind(const std::string& ip_addr_str, std::uint16_t port);
    int listen(DataReceivedCallback data_receive_callback);
    void shutdown();
    void close();

private:
    EventLoop* m_loop = nullptr;
    uv_loop_t* m_uv_loop = nullptr;
};

UdpServer::Impl::Impl(EventLoop& loop) :
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())) {
    auto status = uv_udp_init(m_uv_loop, this);
    // TODO: check status
}

int UdpServer::Impl::bind(const std::string& ip_addr_str, std::uint16_t port) {
    struct sockaddr_in unix_addr;
    uv_ip4_addr(ip_addr_str.c_str(), port, &unix_addr);

    auto status = uv_udp_bind(this, reinterpret_cast<const struct sockaddr*>(&unix_addr), UV_UDP_REUSEADDR);
    return status;
}

int UdpServer::Impl::listen(DataReceivedCallback data_receive_callback) {
    return 0;
}

void UdpServer::Impl::close() {

}

///////////////////////////////////////////  static  ////////////////////////////////////////////

/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpServer::UdpServer(EventLoop& loop) :
    Disposable(loop),
    m_impl(new UdpServer::Impl(loop)) {
}

UdpServer::~UdpServer() {
}

int UdpServer::bind(const std::string& ip_addr_str, std::uint16_t port) {
    return m_impl->bind(ip_addr_str, port);
}

int UdpServer::listen(DataReceivedCallback data_receive_callback) {
    return m_impl->listen(data_receive_callback);
}

void UdpServer::close() {
    return m_impl->close();
}

} // namespace io

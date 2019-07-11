#include "UdpServer.h"

#include <assert.h>

namespace io {

class UdpServer::Impl : public uv_udp_t {
public:
    Impl(EventLoop& loop, UdpServer* parent);

    int bind(const std::string& ip_addr_str, std::uint16_t port);
    void start_receive(DataReceivedCallback data_receive_callback);
    void shutdown();
    void close();

protected:
    // statics
    static void on_data_received(
        uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags);

private:
    EventLoop* m_loop = nullptr;
    uv_loop_t* m_uv_loop = nullptr;
    UdpServer* m_parent = nullptr;

    DataReceivedCallback m_data_receive_callback = nullptr;
};

UdpServer::Impl::Impl(EventLoop& loop, UdpServer* parent) :
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())),
    m_parent(parent) {
    auto status = uv_udp_init(m_uv_loop, this);
    if (status < 0) {
        // TODO: check status
    }

}

int UdpServer::Impl::bind(const std::string& ip_addr_str, std::uint16_t port) {
    struct sockaddr_in unix_addr;
    uv_ip4_addr(ip_addr_str.c_str(), port, &unix_addr);

    auto status = uv_udp_bind(this, reinterpret_cast<const struct sockaddr*>(&unix_addr), UV_UDP_REUSEADDR);
    return status;
}

void UdpServer::Impl::start_receive(DataReceivedCallback data_receive_callback) {
    m_data_receive_callback = data_receive_callback;
    int status = uv_udp_recv_start(this, default_alloc_buffer, on_data_received);
    if (status < 0) {

    }
}

void UdpServer::Impl::close() {

}

///////////////////////////////////////////  static  ////////////////////////////////////////////

void UdpServer::Impl::on_data_received(uv_udp_t* handle,
                                       ssize_t nread,
                                       const uv_buf_t* buf,
                                       const struct sockaddr* addr,
                                       unsigned flags) {
    assert(handle);
    auto& this_ = *reinterpret_cast<UdpServer::Impl*>(handle);
    if (this_.m_data_receive_callback) {
        Status status(nread);

        if (status.ok()) {
            const auto& address = reinterpret_cast<const struct sockaddr_in*>(addr);
            this_.m_data_receive_callback(*this_.m_parent, ntohl(address->sin_addr.s_addr), ntohs(address->sin_port), buf->base, nread, status);
        } else {
            // TODO: implement
        }
    }
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpServer::UdpServer(EventLoop& loop) :
    Disposable(loop),
    m_impl(new UdpServer::Impl(loop, this)) {
}

UdpServer::~UdpServer() {
}

int UdpServer::bind(const std::string& ip_addr_str, std::uint16_t port) {
    return m_impl->bind(ip_addr_str, port);
}

void UdpServer::start_receive(DataReceivedCallback data_receive_callback) {
    return m_impl->start_receive(data_receive_callback);
}

void UdpServer::close() {
    return m_impl->close();
}

} // namespace io

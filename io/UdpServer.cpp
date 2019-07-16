#include "UdpServer.h"
#include "ByteSwap.h"

#include <assert.h>

namespace io {

class UdpServer::Impl : public uv_udp_t {
public:
    Impl(EventLoop& loop, UdpServer& parent);

    Status bind(const std::string& ip_addr_str, std::uint16_t port);
    void start_receive(DataReceivedCallback data_receive_callback);

    void close();
    bool close_with_removal();

protected:
    // statics
    static void on_data_received(
        uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags);
    static void on_close(uv_handle_t* handle);
    static void on_close_with_removal(uv_handle_t* handle);

private:
    EventLoop* m_loop = nullptr;
    uv_loop_t* m_uv_loop = nullptr;

    DataReceivedCallback m_data_receive_callback = nullptr;
};

UdpServer::Impl::Impl(EventLoop& loop, UdpServer& parent) :
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())) {
    auto status = uv_udp_init(m_uv_loop, this);
    if (status < 0) {
        // TODO: check status
    }

    this->data = &parent;
}

Status UdpServer::Impl::bind(const std::string& ip_addr_str, std::uint16_t port) {
    struct sockaddr_in unix_addr;
    uv_ip4_addr(ip_addr_str.c_str(), port, &unix_addr);

    auto uv_status = uv_udp_bind(this, reinterpret_cast<const struct sockaddr*>(&unix_addr), UV_UDP_REUSEADDR);
    Status status(uv_status);
    return status;
}

void UdpServer::Impl::start_receive(DataReceivedCallback data_receive_callback) {
    m_data_receive_callback = data_receive_callback;
    int status = uv_udp_recv_start(this, default_alloc_buffer, on_data_received);
    if (status < 0) {

    }
}

void UdpServer::Impl::close() {
    uv_udp_recv_stop(this);
    uv_close(reinterpret_cast<uv_handle_t*>(this), nullptr);
}

bool UdpServer::Impl::close_with_removal() {
    if (this->data) {
        uv_udp_recv_stop(this);
        uv_close(reinterpret_cast<uv_handle_t*>(this), on_close_with_removal);
        return false; // not ready to remove
    }

    return true;
}

///////////////////////////////////////////  static  ////////////////////////////////////////////

void UdpServer::Impl::on_data_received(uv_udp_t* handle,
                                       ssize_t nread,
                                       const uv_buf_t* buf,
                                       const struct sockaddr* addr,
                                       unsigned flags) {
    assert(handle);
    auto& this_ = *reinterpret_cast<UdpServer::Impl*>(handle);
    auto& parent = *reinterpret_cast<UdpServer*>(this_.data);

    if (this_.m_data_receive_callback) {
        Status status(nread);

        if (status.ok()) {
            if (addr && nread) {
                const auto& address = reinterpret_cast<const struct sockaddr_in*>(addr);
                DataChunk data_chunk(std::shared_ptr<const char>(buf->base, std::default_delete<char[]>()), std::size_t(nread));
                this_.m_data_receive_callback(parent, network_to_host(address->sin_addr.s_addr), network_to_host(address->sin_port), data_chunk, status);
            }
        } else {
            // TODO: implement
        }
    }

    //delete[] buf->base;
}

void UdpServer::Impl::on_close(uv_handle_t* handle) {
    // do nothing
}

void UdpServer::Impl::on_close_with_removal(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<UdpServer::Impl*>(handle);
    auto& parent = *reinterpret_cast<UdpServer*>(this_.data);

    this_.data = nullptr;
    parent.schedule_removal();
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpServer::UdpServer(EventLoop& loop) :
    Disposable(loop),
    m_impl(new UdpServer::Impl(loop, *this)) {
}

UdpServer::~UdpServer() {
}

Status UdpServer::bind(const std::string& ip_addr_str, std::uint16_t port) {
    return m_impl->bind(ip_addr_str, port);
}

void UdpServer::start_receive(DataReceivedCallback data_receive_callback) {
    return m_impl->start_receive(data_receive_callback);
}

void UdpServer::close() {
    return m_impl->close();
}

void UdpServer::schedule_removal() {
    const bool ready_to_remove = m_impl->close_with_removal();
    if (ready_to_remove) {
        Disposable::schedule_removal();
    }
}

} // namespace io

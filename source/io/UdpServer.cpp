#include "UdpServer.h"

#include "ByteSwap.h"
#include "Common.h"
#include "Timer.h"
#include "UdpPeer.h"

#include "detail/UdpImplBase.h"

#include <assert.h>
#include <unordered_map>

namespace io {

class UdpServer::Impl : public detail::UdpImplBase<UdpServer, UdpServer::Impl>{
public:
    Impl(EventLoop& loop, UdpServer& parent);

    Error bind(const std::string& ip_addr_str, std::uint16_t port);
    void start_receive(DataReceivedCallback data_receive_callback);
    void start_receive(DataReceivedCallback receive_callback, std::size_t timeout_ms, PeerTimeoutCallback timeout_callback);

    void close();
    bool close_with_removal();

    void prune_old_peers();

protected:
    // statics
    static void on_data_received(
        uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags);
    static void on_close(uv_handle_t* handle);
    //static void on_close_with_removal(uv_handle_t* handle);

private:
    DataReceivedCallback m_data_receive_callback = nullptr;
    PeerTimeoutCallback m_peer_timeout_callback = nullptr;
    std::size_t m_timeout_ms = 0;

    std::unique_ptr<Timer> m_timer;
};

UdpServer::Impl::Impl(EventLoop& loop, UdpServer& parent) :
    UdpImplBase(loop, parent) {
}

Error UdpServer::Impl::bind(const std::string& ip_addr_str, std::uint16_t port) {
    struct sockaddr_in unix_addr;
    uv_ip4_addr(ip_addr_str.c_str(), port, &unix_addr);

    auto uv_status = uv_udp_bind(&m_udp_handle, reinterpret_cast<const struct sockaddr*>(&unix_addr), UV_UDP_REUSEADDR);
    Error error(uv_status);
    return error;
}

void UdpServer::Impl::start_receive(DataReceivedCallback data_receive_callback) {
    m_data_receive_callback = data_receive_callback;
    int status = uv_udp_recv_start(&m_udp_handle, default_alloc_buffer, on_data_received);
    if (status < 0) {

    }
}

void UdpServer::Impl::start_receive(DataReceivedCallback receive_callback,
                                    std::size_t timeout_ms,
                                    PeerTimeoutCallback timeout_callback) {
    if (m_timeout_ms == 0) {
        // TODO: error
    }

    m_timeout_ms = timeout_ms;
    m_peer_timeout_callback = timeout_callback;
    m_timer.reset(new Timer(*m_loop));
    m_timer->start(m_timeout_ms, m_timeout_ms,
        [this](Timer& timer) {

        }
    );
}

void UdpServer::Impl::prune_old_peers() {
    
}

void UdpServer::Impl::close() {
    uv_udp_recv_stop(&m_udp_handle);
    uv_close(reinterpret_cast<uv_handle_t*>(&m_udp_handle), nullptr);
}

bool UdpServer::Impl::close_with_removal() {
    if (m_udp_handle.data) {
        uv_udp_recv_stop(&m_udp_handle);
        uv_close(reinterpret_cast<uv_handle_t*>(&m_udp_handle), on_close_with_removal);
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
    auto& this_ = *reinterpret_cast<UdpServer::Impl*>(handle->data);
    auto& parent = *this_.m_parent;

    if (this_.m_data_receive_callback) {
        Error error(nread);

        if (!error) {
            if (addr && nread) {
                const auto& address = reinterpret_cast<const struct sockaddr_in*>(addr);
                DataChunk data_chunk(std::shared_ptr<const char>(buf->base, std::default_delete<char[]>()),
                                     std::size_t(nread));
                UdpPeer peer(&this_.m_udp_handle,
                             network_to_host(address->sin_addr.s_addr),
                             network_to_host(address->sin_port));
                this_.m_data_receive_callback(parent, peer, data_chunk, error);
            }
        } else {
            DataChunk data(nullptr, 0);
            // TODO: could address be available here???
            UdpPeer peer(&this_.m_udp_handle, 0, 0);
            // TODO: log here???
            this_.m_data_receive_callback(parent, peer, data, error);
        }
    }
}

void UdpServer::Impl::on_close(uv_handle_t* handle) {
    // do nothing
}
/*
void UdpServer::Impl::on_close_with_removal(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<UdpServer::Impl*>(handle);
    auto& parent = *reinterpret_cast<UdpServer*>(this_.m_udp_handle.data);

    this_.m_udp_handle.data = nullptr;
    parent.schedule_removal();
}
*/
/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpServer::UdpServer(EventLoop& loop) :
    Disposable(loop),
    m_impl(new UdpServer::Impl(loop, *this)) {
}

UdpServer::~UdpServer() {
}

Error UdpServer::bind(const std::string& ip_addr_str, std::uint16_t port) {
    return m_impl->bind(ip_addr_str, port);
}

void UdpServer::start_receive(DataReceivedCallback data_receive_callback) {
    return m_impl->start_receive(data_receive_callback);
}

void UdpServer::start_receive(DataReceivedCallback receive_callback, std::size_t timeout_ms, PeerTimeoutCallback timeout_callback) {
    return m_impl->start_receive(receive_callback, timeout_ms, timeout_callback);
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

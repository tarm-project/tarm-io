#include "UdpServer.h"

#include "BacklogWithTimeout.h"
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
    Error start_receive(const std::string& ip_addr_str, std::uint16_t port, DataReceivedCallback data_receive_callback);
    Error start_receive(const std::string& ip_addr_str, std::uint16_t port, NewPeerCallback new_peer_callback, DataReceivedCallback receive_callback, std::size_t timeout_ms, PeerTimeoutCallback timeout_callback);

    void close();
    bool close_with_removal();

    bool peer_bookkeeping_enabled() const;

    // TODO: move
    static void free_udp_peer(UdpPeer* peer) {
        peer->unref();
    }
// m_peer_timeout_callback(*m_parent, *it->second);
protected:
    // statics
    static void on_data_received(
        uv_udp_t* handle, ssize_t nread, const uv_buf_t* uv_buf, const struct sockaddr* addr, unsigned flags);
    static void on_close(uv_handle_t* handle);

private:
    NewPeerCallback m_new_peer_callback = nullptr;
    DataReceivedCallback m_data_receive_callback = nullptr;
    PeerTimeoutCallback m_peer_timeout_callback = nullptr;

    std::unordered_map<std::uint64_t, std::shared_ptr<UdpPeer>> m_peers;
    std::unique_ptr<BacklogWithTimeout<std::shared_ptr<UdpPeer>>> m_peers_backlog;
};

UdpServer::Impl::Impl(EventLoop& loop, UdpServer& parent) :
    UdpImplBase(loop, parent) {
}

Error UdpServer::Impl::bind(const std::string& ip_addr_str, std::uint16_t port) {
    struct sockaddr_in unix_addr;
    uv_ip4_addr(ip_addr_str.c_str(), port, &unix_addr);

    auto uv_status = uv_udp_bind(m_udp_handle.get(), reinterpret_cast<const struct sockaddr*>(&unix_addr), UV_UDP_REUSEADDR);
    Error error(uv_status);
    return error;
}

Error UdpServer::Impl::start_receive(const std::string& ip_addr_str, std::uint16_t port, DataReceivedCallback data_receive_callback) {
    Error bind_error = bind(ip_addr_str, port);
    if (bind_error) {
        return bind_error;
    }

    m_data_receive_callback = data_receive_callback;
    int status = uv_udp_recv_start(m_udp_handle.get(), default_alloc_buffer, on_data_received);
    if (status < 0) {

    }

    return Error(0);
}

Error UdpServer::Impl::start_receive(const std::string& ip_addr_str,
                                     std::uint16_t port,
                                     NewPeerCallback new_peer_callback,
                                     DataReceivedCallback receive_callback,
                                     std::size_t timeout_ms,
                                     PeerTimeoutCallback timeout_callback) {
    if (timeout_ms == 0) {
        // TODO: error
    }

    m_new_peer_callback = new_peer_callback;
    m_peer_timeout_callback = timeout_callback;

    // TODO: bind instead of lambdas
    auto on_expired = [this](io::BacklogWithTimeout<std::shared_ptr<UdpPeer>>&, const std::shared_ptr<UdpPeer>& item) {
        m_peer_timeout_callback(*m_parent, *item, Error(0));

        // TODO: store in peer or make a function for this
        const std::uint64_t peer_id = std::uint64_t(host_to_network(item->port())) << 16 | std::uint64_t(host_to_network(item->address()));
        auto it = m_peers.find(peer_id);
        if (it != m_peers.end()) {
            m_peers.erase(it);
        } else {
            // error handling
        }
    };

    auto time_getter = [](const std::shared_ptr<UdpPeer>& item) -> std::uint64_t {
        return item->last_packet_time_ns();
    };

    m_peers_backlog.reset(new BacklogWithTimeout<std::shared_ptr<UdpPeer>>(*m_loop, timeout_ms, on_expired, time_getter));

    return start_receive(ip_addr_str, port, receive_callback);
}

void UdpServer::Impl::close() {
    uv_udp_recv_stop(m_udp_handle.get());
    uv_close(reinterpret_cast<uv_handle_t*>(m_udp_handle.get()), nullptr);
}

bool UdpServer::Impl::close_with_removal() {
    if (m_udp_handle->data) {
        uv_udp_recv_stop(m_udp_handle.get());
        uv_close(reinterpret_cast<uv_handle_t*>(m_udp_handle.get()), on_close_with_removal);
        return false; // not ready to remove
    }

    m_peers.clear();

    return true;
}

bool UdpServer::Impl::peer_bookkeeping_enabled() const {
    return m_peers_backlog.get() != nullptr;
}

///////////////////////////////////////////  static  ////////////////////////////////////////////

void UdpServer::Impl::on_data_received(uv_udp_t* handle,
                                       ssize_t nread,
                                       const uv_buf_t* uv_buf,
                                       const struct sockaddr* addr,
                                       unsigned flags) {
    assert(handle);
    auto& this_ = *reinterpret_cast<UdpServer::Impl*>(handle->data);
    auto& parent = *this_.m_parent;

    // TODO: need some mechanism to reuse memory
    std::shared_ptr<const char> buf(uv_buf->base, std::default_delete<char[]>());

    if (this_.m_data_receive_callback) {
        Error error(nread);

        if (!error) {
            if (addr && nread) {
                const auto& address = reinterpret_cast<const struct sockaddr_in*>(addr);

                DataChunk data_chunk(buf, std::size_t(nread));
                if (this_.peer_bookkeeping_enabled()) {
                    std::uint64_t peer_id = std::uint64_t(address->sin_port) << 16 | std::uint64_t(address->sin_addr.s_addr);
                    auto& peer_ptr = this_.m_peers[peer_id];
                    if (!peer_ptr.get()) {
                        peer_ptr.reset(new UdpPeer(*this_.m_loop,
                                                   this_.m_udp_handle.get(),
                                                   network_to_host(address->sin_addr.s_addr),
                                                   network_to_host(address->sin_port)),
                                       free_udp_peer);
                        peer_ptr->ref(); // Holding extra reference to prevent removal by ref/unref mechanics

                        peer_ptr->set_last_packet_time_ns(uv_hrtime());
                        this_.m_peers_backlog->add_item(peer_ptr);

                        if (this_.m_new_peer_callback) {
                            this_.m_new_peer_callback(parent, *peer_ptr.get(), Error(0));
                        }
                    }
                    this_.m_data_receive_callback(parent, *peer_ptr, data_chunk, error);

                    peer_ptr->set_last_packet_time_ns(uv_hrtime());
                } else {
                    /*
                    UdpPeer peer(*this_.m_loop,
                                 this_.m_udp_handle.get(),
                                 network_to_host(address->sin_addr.s_addr),
                                 network_to_host(address->sin_port));
                                 //*/

                    // Ref/Unref semantics here was added to prolong lifetime of oneshot UdpPeer objects
                    // and to allow call send data in receive callback for UdpServer without peers tracking.
                    auto peer = new UdpPeer(*this_.m_loop,
                                 this_.m_udp_handle.get(),
                                 network_to_host(address->sin_addr.s_addr),
                                 network_to_host(address->sin_port));
                    peer->ref();
                    this_.m_data_receive_callback(parent, *peer, data_chunk, error);
                    peer->unref();
                }
            }
        } else {
            DataChunk data(nullptr, 0);
            // TODO: could address be available here???
            UdpPeer peer(*this_.m_loop, this_.m_udp_handle.get(), 0, 0);
            // TODO: log here???
            this_.m_data_receive_callback(parent, peer, data, error);
        }
    }
}

void UdpServer::Impl::on_close(uv_handle_t* handle) {
    // do nothing
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpServer::UdpServer(EventLoop& loop) :
    Removable(loop),
    m_impl(new UdpServer::Impl(loop, *this)) {
}

UdpServer::~UdpServer() {
}

Error UdpServer::start_receive(const std::string& ip_addr_str, std::uint16_t port, DataReceivedCallback data_receive_callback) {
    return m_impl->start_receive(ip_addr_str, port, data_receive_callback);
}

Error UdpServer::start_receive(const std::string& ip_addr_str, std::uint16_t port, NewPeerCallback new_peer_callback, DataReceivedCallback receive_callback, std::size_t timeout_ms, PeerTimeoutCallback timeout_callback) {
    return m_impl->start_receive(ip_addr_str, port, new_peer_callback, receive_callback, timeout_ms, timeout_callback);
}

Error UdpServer::start_receive(const std::string& ip_addr_str, std::uint16_t port, DataReceivedCallback receive_callback, std::size_t timeout_ms, PeerTimeoutCallback timeout_callback) {
    return m_impl->start_receive(ip_addr_str, port, nullptr, receive_callback, timeout_ms, timeout_callback);
}

void UdpServer::close() {
    return m_impl->close();
}

void UdpServer::schedule_removal() {
    const bool ready_to_remove = m_impl->close_with_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

} // namespace io

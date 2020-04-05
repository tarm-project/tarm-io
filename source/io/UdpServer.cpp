#include "UdpServer.h"

#include "BacklogWithTimeout.h"
#include "ByteSwap.h"
#include "Timer.h"
#include "UdpPeer.h"

#include "detail/Common.h"
#include "detail/PeerId.h"
#include "detail/UdpImplBase.h"

#include <iostream>
#include <assert.h>
#include <unordered_map>

namespace io {

class UdpServer::Impl : public detail::UdpImplBase<UdpServer, UdpServer::Impl>{
public:
    Impl(EventLoop& loop, UdpServer& parent);

    Error bind(const Endpoint& endpoint);
    Error start_receive(const Endpoint& endpoint, DataReceivedCallback data_receive_callback);
    Error start_receive(const Endpoint& endpoint, NewPeerCallback new_peer_callback, DataReceivedCallback receive_callback, std::size_t timeout_ms, PeerTimeoutCallback timeout_callback);

    void close();
    bool close_with_removal();

    bool peer_bookkeeping_enabled() const;

    void close_peer(UdpPeer& peer, std::size_t inactivity_timeout_ms);

protected:
    // statics
    static void on_data_received(
        uv_udp_t* handle, ssize_t nread, const uv_buf_t* uv_buf, const struct sockaddr* addr, unsigned flags);
    static void on_close(uv_handle_t* handle);

    static void free_udp_peer(UdpPeer* peer);

private:
    NewPeerCallback m_new_peer_callback = nullptr;
    DataReceivedCallback m_data_receive_callback = nullptr;
    PeerTimeoutCallback m_peer_timeout_callback = nullptr;

    std::unordered_map<detail::PeerId, std::shared_ptr<UdpPeer>> m_peers;
    std::unordered_map<detail::PeerId, std::unique_ptr<Timer, typename Timer::DefaultDelete>> m_inactive_peers;
    std::unique_ptr<BacklogWithTimeout<std::shared_ptr<UdpPeer>>> m_peers_backlog;
};

UdpServer::Impl::Impl(EventLoop& loop, UdpServer& parent) :
    UdpImplBase(loop, parent) {
}

Error UdpServer::Impl::bind(const Endpoint& endpoint) {
    if (endpoint.type() == Endpoint::UNDEFINED) {
        return Error(StatusCode::INVALID_ARGUMENT);
    }

    // TODO: UV_UDP_REUSEADDR ????
    auto uv_status = uv_udp_bind(m_udp_handle.get(), reinterpret_cast<const struct sockaddr*>(endpoint.raw_endpoint()), 0);
    return Error(uv_status);
}

Error UdpServer::Impl::start_receive(const Endpoint& endpoint, DataReceivedCallback data_receive_callback) {
    IO_LOG(m_loop, TRACE, m_parent, "");

    Error bind_error = bind(endpoint);
    if (bind_error) {
        return bind_error;
    }

    m_data_receive_callback = data_receive_callback;

    Error receive_start_error = uv_udp_recv_start(m_udp_handle.get(), detail::default_alloc_buffer, on_data_received);
    if (receive_start_error) {
        return receive_start_error;
    }

    return Error(0);
}

Error UdpServer::Impl::start_receive(const Endpoint& endpoint,
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
        m_peer_timeout_callback(*item, Error(0));

        auto it = m_peers.find(item->id());
        if (it != m_peers.end()) {
            m_peers.erase(it);
        } else {
            // TODO: error handling
        }
    };

    auto time_getter = [](const std::shared_ptr<UdpPeer>& item) -> std::uint64_t {
        return item->last_packet_time();
    };

    m_peers_backlog.reset(new BacklogWithTimeout<std::shared_ptr<UdpPeer>>(*m_loop, timeout_ms, on_expired, time_getter, &uv_hrtime));

    return start_receive(endpoint, receive_callback);
}

void UdpServer::Impl::close() {
    IO_LOG(m_loop, TRACE, m_parent, "");

    uv_udp_recv_stop(m_udp_handle.get());
    uv_close(reinterpret_cast<uv_handle_t*>(m_udp_handle.get()), nullptr);
}

bool UdpServer::Impl::close_with_removal() {
    if (m_udp_handle->data) {
        Error error = uv_udp_recv_stop(m_udp_handle.get());
        uv_close(reinterpret_cast<uv_handle_t*>(m_udp_handle.get()), on_close_with_removal);
        return false; // not ready to remove
    }

    m_peers.clear();
    m_inactive_peers.clear();

    return true;
}

bool UdpServer::Impl::peer_bookkeeping_enabled() const {
    return m_peers_backlog.get() != nullptr;
}

void UdpServer::Impl::close_peer(UdpPeer& peer, std::size_t inactivity_timeout_ms) {
    IO_LOG(m_loop, TRACE, m_parent, "");


    const auto peer_id = peer.id();

    auto it = m_inactive_peers.find(peer_id);
    if (it != m_inactive_peers.end()) {
        return;
    }

    auto timer = new Timer(*m_loop);
    m_inactive_peers.insert(std::make_pair(peer_id,
                                           std::unique_ptr<Timer,
                                                           typename Timer::DefaultDelete>
                                                          (timer, Timer::default_delete())));
    timer->start(inactivity_timeout_ms,
                 [this, peer_id] (Timer& timer) {
                     m_inactive_peers.erase(peer_id);
                 });

    auto active_it = m_peers.find(peer_id);
    if (active_it == m_peers.end()) {
        // TODO: log error
        return;
    }

    m_peers_backlog->remove_item(active_it->second);
    m_peers.erase(active_it);
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
                detail::PeerId peer_id{addr};

                DataChunk data_chunk(buf, std::size_t(nread));
                if (this_.peer_bookkeeping_enabled()) {
                    auto inative_peer_it = this_.m_inactive_peers.find(peer_id);
                    if (inative_peer_it != this_.m_inactive_peers.end()) {
                        // TODO: fixme log output
                        //IO_LOG(this_.m_loop, TRACE, &parent, "Peer", peer_id, "is inactive, ignoring packet");
                        return;
                    }

                    auto& peer_ptr = this_.m_peers[peer_id];
                    if (!peer_ptr.get()) {
                        // TODO: fixme log output
                        //IO_LOG(this_.m_loop, TRACE, &parent, "New tracked peer id:", peer_id);

                        peer_ptr.reset(new UdpPeer(*this_.m_loop,
                                                   *this_.m_parent,
                                                   this_.m_udp_handle.get(),
                                                   {addr},
                                                   peer_id),
                                       free_udp_peer); // Ref count is == 1 here

                        peer_ptr->set_last_packet_time(::uv_hrtime());
                        this_.m_peers_backlog->add_item(peer_ptr);

                        if (this_.m_new_peer_callback) {
                            this_.m_new_peer_callback(*peer_ptr.get(), Error(0));
                        }
                    }

                    peer_ptr->set_last_packet_time(::uv_hrtime());

                    // This should be the last because peer may be reseted in callback
                    this_.m_data_receive_callback(*peer_ptr, data_chunk, error);
                } else {
                    // TODO: fixme log output
                    //IO_LOG(this_.m_loop, TRACE, &parent, "New untracked peer id:", peer_id);

                    // Ref/Unref semantics here was added to prolong lifetime of oneshot UdpPeer objects
                    // and to allow call send data in receive callback for UdpServer without peers tracking.
                    auto peer = new UdpPeer(*this_.m_loop,
                                 *this_.m_parent,
                                 this_.m_udp_handle.get(),
                                 {addr},
                                 peer_id); // Ref count is == 1 here
                    this_.m_data_receive_callback(*peer, data_chunk, error);
                    peer->unref();
                }
            }
        } else {
            DataChunk data(nullptr, 0);
            // TODO: could address be available here???
            UdpPeer peer(*this_.m_loop, *this_.m_parent, this_.m_udp_handle.get(), Endpoint{0u, 0u}, 0);

            IO_LOG(this_.m_loop, ERROR, &parent, "failed to receive UDP packet", error.string());
            this_.m_data_receive_callback(peer, data, error);
        }
    }
}

void UdpServer::Impl::on_close(uv_handle_t* handle) {
    // do nothing
}

void UdpServer::Impl::free_udp_peer(UdpPeer* peer) {
    peer->unref();
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpServer::UdpServer(EventLoop& loop) :
    Removable(loop),
    m_impl(new UdpServer::Impl(loop, *this)) {
}

UdpServer::~UdpServer() {
}

Error UdpServer::start_receive(const Endpoint& endpoint, DataReceivedCallback data_receive_callback) {
    return m_impl->start_receive(endpoint, data_receive_callback);
}

Error UdpServer::start_receive(const Endpoint& endpoint, NewPeerCallback new_peer_callback, DataReceivedCallback receive_callback, std::size_t timeout_ms, PeerTimeoutCallback timeout_callback) {
    return m_impl->start_receive(endpoint, new_peer_callback, receive_callback, timeout_ms, timeout_callback);
}

Error UdpServer::start_receive(const Endpoint& endpoint, DataReceivedCallback receive_callback, std::size_t timeout_ms, PeerTimeoutCallback timeout_callback) {
    return m_impl->start_receive(endpoint, nullptr, receive_callback, timeout_ms, timeout_callback);
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

void UdpServer::close_peer(UdpPeer& peer, std::size_t inactivity_timeout_ms) {
    return m_impl->close_peer(peer, inactivity_timeout_ms);
}

BufferSizeResult UdpServer::receive_buffer_size() const {
    return m_impl->receive_buffer_size();
}

BufferSizeResult UdpServer::send_buffer_size() const {
    return m_impl->send_buffer_size();
}

Error UdpServer::set_receive_buffer_size(std::size_t size) {
    return m_impl->set_receive_buffer_size(size);
}

Error UdpServer::set_send_buffer_size(std::size_t size) {
    return m_impl->set_send_buffer_size(size);
}

} // namespace io

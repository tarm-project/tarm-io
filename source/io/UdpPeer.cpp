#include "UdpPeer.h"

#include "UdpServer.h"

#include "ByteSwap.h"
#include "detail/UdpClientImplBase.h"
#include "detail/PeerId.h"

#include "detail/Common.h"

namespace io {

class UdpPeer::Impl : public detail::UdpClientImplBase<UdpPeer, UdpPeer::Impl> {
public:
    Impl(EventLoop& loop, UdpServer& server, void* udp_handle, const Endpoint& endpoint, const detail::PeerId& id, UdpPeer& parent);

    std::uint32_t address();
    std::uint16_t port();

    UdpServer& server();
    const UdpServer& server() const;

    void close(std::size_t inactivity_timeout_ms);

    const detail::PeerId& id() const;

private:
    UdpServer* m_server = nullptr;
    const detail::PeerId m_id;
};

UdpPeer::Impl::Impl(EventLoop& loop, UdpServer& server, void* udp_handle, const Endpoint& endpoint, const detail::PeerId& id, UdpPeer& parent) :
    UdpClientImplBase(loop, parent, parent, reinterpret_cast<uv_udp_t*>(udp_handle)),
    m_server(&server),
    m_id(id) {
    m_destination_endpoint = endpoint;
}

// TODO: ipv6
std::uint32_t UdpPeer::Impl::address() {
    auto& unix_addr = *reinterpret_cast<sockaddr_in*>(m_destination_endpoint.raw_endpoint());
    return network_to_host(unix_addr.sin_addr.s_addr);
}

std::uint16_t UdpPeer::Impl::port() {
    auto& unix_addr = *reinterpret_cast<sockaddr_in*>(m_destination_endpoint.raw_endpoint());
    return network_to_host(unix_addr.sin_port);
}

UdpServer& UdpPeer::Impl::server() {
    return *m_server;
}

const UdpServer& UdpPeer::Impl::server() const {
    return *m_server;
}

void UdpPeer::Impl::close(std::size_t inactivity_timeout_ms) {
    m_server->close_peer(*m_parent, inactivity_timeout_ms);
    m_udp_handle = nullptr;
}

const detail::PeerId& UdpPeer::Impl::id() const {
    return m_id;
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpPeer::UdpPeer(EventLoop& loop, UdpServer& server, void* udp_handle, const Endpoint& endpoint, const detail::PeerId& id) :
    RefCounted(loop),
    m_impl(new Impl(loop, server, udp_handle, endpoint, id, *this)) {
}

UdpPeer::~UdpPeer() {
}

void UdpPeer::set_last_packet_time(std::uint64_t time) {
    return m_impl->set_last_packet_time(time);
}

std::uint64_t UdpPeer::last_packet_time() const {
    return m_impl->last_packet_time();
}

std::uint32_t UdpPeer::address() const {
    return m_impl->address();
}

std::uint16_t UdpPeer::port() const {
    return m_impl->port();
}

void UdpPeer::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback) {
    return m_impl->send_data(buffer, size, callback);
}

void UdpPeer::send_data(const std::string& message, EndSendCallback callback) {
    return m_impl->send_data(message, callback);
}

bool UdpPeer::is_open() const {
    return m_impl->is_open();
}

UdpServer& UdpPeer::server() {
    return m_impl->server();
}

const UdpServer& UdpPeer::server() const {
    return m_impl->server();
}

void UdpPeer::close(std::size_t inactivity_timeout_ms) {
    return m_impl->close(inactivity_timeout_ms);
}

const detail::PeerId& UdpPeer::id() const {
    return m_impl->id();
}

} // namespace io

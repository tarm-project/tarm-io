#include "UdpPeer.h"

#include "ByteSwap.h"
#include "detail/UdpClientImplBase.h"

#include "Common.h"

namespace io {

class UdpPeer::Impl : public detail::UdpClientImplBase<UdpPeer, UdpPeer::Impl> {
public:
    Impl(EventLoop& loop, UdpServer& server, void* udp_handle, std::uint32_t address, std::uint16_t port, UdpPeer& parent);

    std::uint32_t address();
    std::uint16_t port();

    void set_last_packet_time_ns(std::uint64_t time);
    std::uint64_t last_packet_time_ns() const;

    UdpServer& server();
    const UdpServer& server() const;

private:
    UdpServer* m_server = nullptr;
    std::uint64_t m_last_packet_time_ns = 0;
};

UdpPeer::Impl::Impl(EventLoop& loop, UdpServer& server, void* udp_handle, std::uint32_t address, std::uint16_t port, UdpPeer& parent) :
    UdpClientImplBase(loop, parent, parent, reinterpret_cast<uv_udp_t*>(udp_handle)),
    m_server(&server) {
    auto& unix_addr = *reinterpret_cast<sockaddr_in*>(&m_raw_unix_addr);
    unix_addr.sin_family = AF_INET;
    unix_addr.sin_port = host_to_network(port);
    unix_addr.sin_addr.s_addr = host_to_network(address);
}

std::uint32_t UdpPeer::Impl::address() {
    auto& unix_addr = *reinterpret_cast<sockaddr_in*>(&m_raw_unix_addr);
    return network_to_host(unix_addr.sin_addr.s_addr);
}

std::uint16_t UdpPeer::Impl::port() {
    auto& unix_addr = *reinterpret_cast<sockaddr_in*>(&m_raw_unix_addr);
    return network_to_host(unix_addr.sin_port);
}

void UdpPeer::Impl::set_last_packet_time_ns(std::uint64_t time) {
    m_last_packet_time_ns = time;
}

std::uint64_t UdpPeer::Impl::last_packet_time_ns() const {
    return m_last_packet_time_ns;
}

UdpServer& UdpPeer::Impl::server() {
    return *m_server;
}

const UdpServer& UdpPeer::Impl::server() const {
    return *m_server;
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpPeer::UdpPeer(EventLoop& loop, UdpServer& server, void* udp_handle, std::uint32_t address, std::uint16_t port) :
    RefCounted(loop),
    m_impl(new Impl(loop, server, udp_handle, address, port, *this)) {
}

UdpPeer::~UdpPeer() {
}

void UdpPeer::set_last_packet_time_ns(std::uint64_t time) {
    return m_impl->set_last_packet_time_ns(time);
}

std::uint64_t UdpPeer::last_packet_time_ns() const {
    return m_impl->last_packet_time_ns();
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
    return true; // TODO: revise this
}

UdpServer& UdpPeer::server() {
    return m_impl->server();
}

const UdpServer& UdpPeer::server() const {
    return m_impl->server();
}

} // namespace io

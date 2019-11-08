#include "UdpPeer.h"

#include "detail/UdpClientImplBase.h"

#include "Common.h"

namespace io {

class UdpPeer::Impl : public detail::UdpClientImplBase<UdpPeer, UdpPeer::Impl> {
public:
    Impl(EventLoop& loop, UdpPeer& parent, void* udp_handle, std::uint32_t address, std::uint16_t port);

    std::uint32_t address();
    std::uint16_t port();

    void set_last_packet_time_ns(std::uint64_t time);

private:
    std::uint32_t m_address;
    std::uint16_t m_port;

    std::uint64_t m_last_packet_time_ns = 0;
};

UdpPeer::Impl::Impl(EventLoop& loop, UdpPeer& parent, void* udp_handle, std::uint32_t address, std::uint16_t port) :
    UdpClientImplBase(loop, parent, reinterpret_cast<uv_udp_t*>(udp_handle)),
    m_address(address),
    m_port(port) {
}

std::uint32_t UdpPeer::Impl::address() {
    return m_address;
}

std::uint16_t UdpPeer::Impl::port() {
    return m_port;
}

void UdpPeer::Impl::set_last_packet_time_ns(std::uint64_t time) {
    m_last_packet_time_ns = time;
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpPeer::UdpPeer(EventLoop& loop, void* udp_handle, std::uint32_t address, std::uint16_t port) :
    m_impl(new Impl(loop, *this, udp_handle, address, port)) {
}

UdpPeer::~UdpPeer() {

}

void UdpPeer::set_last_packet_time_ns(std::uint64_t time) {
    return m_impl->set_last_packet_time_ns(time);
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

} // namespace io

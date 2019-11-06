#include "UdpPeer.h"

#include "Common.h"

namespace io {

class UdpPeer::Impl {
public:
    Impl(void* udp_handle, std::uint32_t address, std::uint16_t port);

    std::uint32_t address();
    std::uint16_t port();

    void set_last_packet_time_ns(std::uint64_t time);

private:
    uv_udp_t* m_udp_handle;
    std::uint32_t m_address;
    std::uint16_t m_port;

    std::uint64_t m_last_packet_time_ns = 0;
};

UdpPeer::Impl::Impl(void* udp_handle, std::uint32_t address, std::uint16_t port) :
    m_udp_handle(reinterpret_cast<uv_udp_t*>(udp_handle)),
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

UdpPeer::UdpPeer(void* udp_handle, std::uint32_t address, std::uint16_t port) :
    m_impl(new Impl(udp_handle, address, port)) {
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


} // namespace io

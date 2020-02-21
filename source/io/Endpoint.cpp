#include "Endpoint.h"

#include "ByteSwap.h"
#include "Convert.h"
#include "Error.h"

#include <uv.h>

#include <assert.h>

namespace io {

class Endpoint::Impl {
public:
    Impl();
    Impl(const std::string& address, std::uint16_t port);
    Impl(std::array<std::uint8_t, 4> address, std::uint16_t port);
    Impl(std::array<std::uint8_t, 16> address, std::uint16_t port);
    Impl(std::uint32_t address, std::uint16_t port);

    std::string as_string() const;
    std::uint16_t port() const;
    Type type() const;

    void* raw_endpoint();
    const void* raw_endpoint() const;

private:
    ::sockaddr_storage m_address_storage;
    Type m_type = UNDEFINED;
};

Endpoint::Impl::Impl() {
}

Endpoint::Impl::Impl(const std::string& address, std::uint16_t port) {
    auto addr = reinterpret_cast<::sockaddr_in*>(&m_address_storage);

    Error address_error = uv_ip4_addr(address.c_str(), port, addr);
    if (address_error) {
        return;
    }

    // TODO: IPV6
    m_type = IP_V4;
}

Endpoint::Impl::Impl(std::array<std::uint8_t, 4> address, std::uint16_t port) {
    m_type = IP_V4;
    auto addr = reinterpret_cast<::sockaddr_in*>(&m_address_storage);
    addr->sin_family = AF_INET;
    addr->sin_port = host_to_network(port);
    addr->sin_addr.s_addr = std::uint32_t(address[0]) << 24 |
                            std::uint32_t(address[1]) << 16 |
                            std::uint32_t(address[2]) << 8  |
                            std::uint32_t(address[3]);
}

Endpoint::Impl::Impl(std::array<std::uint8_t, 16> address, std::uint16_t port) {
    assert(false);
    // TODO:
}

Endpoint::Impl::Impl(std::uint32_t address, std::uint16_t port) {
    m_type = IP_V4;
    auto addr = reinterpret_cast<::sockaddr_in*>(&m_address_storage);
    addr->sin_family = AF_INET;
    addr->sin_port = host_to_network(port);
    addr->sin_addr.s_addr = host_to_network(address);
}

std::string Endpoint::Impl::as_string() const {
    return "";
}

std::uint16_t Endpoint::Impl::port() const {
    // TODO: IPV6?
    auto addr = reinterpret_cast<const ::sockaddr_in*>(&m_address_storage);
    return network_to_host(addr->sin_port);
}

Endpoint::Type Endpoint::Impl::type() const {
    return m_type;
}

void* Endpoint::Impl::raw_endpoint() {
    return &m_address_storage;
}

const void* Endpoint::Impl::raw_endpoint() const {
    return &m_address_storage;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

Endpoint::Endpoint() :
    m_impl(new Impl()) {
}

Endpoint::Endpoint(const std::string& address, std::uint16_t port) :
    m_impl(new Impl(address, port)) {
}

Endpoint::Endpoint(std::array<std::uint8_t, 4> address, std::uint16_t port) :
    m_impl(new Impl(address, port)) {
}

Endpoint::Endpoint(std::array<std::uint8_t, 16> address, std::uint16_t port) :
    m_impl(new Impl(address, port)) {
}

Endpoint::Endpoint(std::uint32_t address, std::uint16_t port) :
    m_impl(new Impl(address, port)) {
}

Endpoint::~Endpoint() {
}

std::string Endpoint::as_string() const {
    return m_impl->as_string();
}

std::uint16_t Endpoint::port() const {
    return m_impl->port();
}

Endpoint::Type Endpoint::type() const {
    return m_impl->type();
}

void* Endpoint::raw_endpoint() {
    return m_impl->raw_endpoint();
}

const void* Endpoint::raw_endpoint() const {
    return m_impl->raw_endpoint();
}

} // namespace io

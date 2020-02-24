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
    Impl(std::uint32_t address, std::uint16_t port);
    Impl(const std::uint8_t* address_bytes, std::size_t _address_size, std::uint16_t port);

    Impl(const Impl& other);

    std::string address_string() const;
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

Endpoint::Impl::Impl(const Impl& other) :
   m_address_storage(other.m_address_storage),
   m_type(other.m_type) {
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

/*
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
*/

Endpoint::Impl::Impl(const std::uint8_t* address_bytes, std::size_t address_size, std::uint16_t port) {
    auto addr = reinterpret_cast<::sockaddr_in*>(&m_address_storage);
    addr->sin_port = host_to_network(port);

    if (address_size == 4) {
            m_type = IP_V4;
            auto addr = reinterpret_cast<::sockaddr_in*>(&m_address_storage);
            addr->sin_family = AF_INET;
            addr->sin_addr.s_addr = std::uint32_t(address_bytes[3]) << 24 |
                                    std::uint32_t(address_bytes[2]) << 16 |
                                    std::uint32_t(address_bytes[1]) << 8  |
                                    std::uint32_t(address_bytes[0]);
    }
}

Endpoint::Impl::Impl(std::uint32_t address, std::uint16_t port) {
    m_type = IP_V4;
    auto addr = reinterpret_cast<::sockaddr_in*>(&m_address_storage);
    addr->sin_family = AF_INET;
    addr->sin_port = host_to_network(port);
    addr->sin_addr.s_addr = host_to_network(address);
}

std::string Endpoint::Impl::address_string() const {
    if (m_type == IP_V4) {
        const auto addr = reinterpret_cast<const ::sockaddr_in*>(&m_address_storage);
        return ip4_addr_to_string(network_to_host(addr->sin_addr.s_addr));
    }

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

Endpoint::Endpoint(std::uint32_t address, std::uint16_t port) :
    m_impl(new Impl(address, port)) {

}

Endpoint::Endpoint(const std::uint8_t* address_bytes, std::size_t address_size, std::uint16_t port) :
    m_impl(new Impl(address_bytes, address_size, port)){
}

Endpoint::Endpoint(std::initializer_list<std::uint8_t> address_bytes, std::uint16_t port) :
    m_impl(new Impl(&*address_bytes.begin(), address_bytes.size(), port)) {
}

Endpoint::~Endpoint() {
}

Endpoint& Endpoint::operator=(const Endpoint& other) {
    (*m_impl.get()) = (*other.m_impl.get());
    return *this;
}

Endpoint::Endpoint(const Endpoint& other) :
    m_impl(new Impl(*other.m_impl.get())) {
}

std::string Endpoint::address_string() const {
    return m_impl->address_string();
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

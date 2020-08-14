/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "net/Endpoint.h"

#include "ByteSwap.h"
#include "Convert.h"
#include "Error.h"

#include <uv.h>

#include <cstring>
#include <assert.h>

namespace tarm {
namespace io {
namespace net {

class Endpoint::Impl {
public:
    Impl();
    Impl(const ::sockaddr* address);
    Impl(const std::string& address, std::uint16_t port);
    Impl(std::uint32_t address, std::uint16_t port);
    Impl(const std::uint8_t* address_bytes, std::size_t _address_size, std::uint16_t port);

    Impl(const Impl& other);

    std::string address_string() const;
    std::uint16_t port() const;
    Type type() const;

    void* raw_endpoint();
    const void* raw_endpoint() const;

    std::uint32_t ipv4_addr() const;

    void clear();

private:
    ::sockaddr_storage m_address_storage;
    Type m_type = UNDEFINED;
};

Endpoint::Impl::Impl() {
    std::memset(&m_address_storage, 0, sizeof(::sockaddr_storage));
}

Endpoint::Impl::Impl(const Impl& other) :
   m_address_storage(other.m_address_storage),
   m_type(other.m_type) {
}

Endpoint::Impl::Impl(const ::sockaddr* address) :
    Impl() {

    if (address->sa_family == AF_INET) {
        m_type = IP_V4;
        std::memcpy(&m_address_storage, reinterpret_cast<const ::sockaddr_in*>(address), sizeof(::sockaddr_in));
    } else if (address->sa_family == AF_INET6) {
        m_type = IP_V6;
        std::memcpy(&m_address_storage, reinterpret_cast<const ::sockaddr_in6*>(address), sizeof(::sockaddr_in6));
    }
}

Endpoint::Impl::Impl(const std::string& address, std::uint16_t port) :
    Impl() {

    if (address.empty()) {
        return;
    }

    if (address.find(":") != std::string::npos) {
        auto addr = reinterpret_cast<::sockaddr_in6*>(&m_address_storage);
        Error address_error = uv_ip6_addr(address.c_str(), port, addr);
        if (address_error) {
            return;
        }
        m_type = IP_V6;
    } else {
        auto addr = reinterpret_cast<::sockaddr_in*>(&m_address_storage);
        Error address_error = uv_ip4_addr(address.c_str(), port, addr);
        if (address_error) {
            return;
        }
        m_type = IP_V4;
    }
}

Endpoint::Impl::Impl(const std::uint8_t* address_bytes, std::size_t address_size, std::uint16_t port) :
    Impl() {

    if (address_size == 4) {
        m_type = IP_V4;
        auto addr = reinterpret_cast<::sockaddr_in*>(&m_address_storage);
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = std::uint32_t(address_bytes[3]) << 24 |
                                std::uint32_t(address_bytes[2]) << 16 |
                                std::uint32_t(address_bytes[1]) << 8  |
                                std::uint32_t(address_bytes[0]);
        addr->sin_port = host_to_network(port);
    } else if (address_size == 16) {
        m_type = IP_V6;
        auto addr = reinterpret_cast<::sockaddr_in6*>(&m_address_storage);
        addr->sin6_family = AF_INET6;
        std::memcpy(&addr->sin6_addr, address_bytes, 16);
        addr->sin6_port = host_to_network(port);
    }
}

Endpoint::Impl::Impl(std::uint32_t address, std::uint16_t port) :
    Impl() {
    m_type = IP_V4;
    auto addr = reinterpret_cast<::sockaddr_in*>(&m_address_storage);
    addr->sin_family = AF_INET;
    addr->sin_port = host_to_network(port);
    addr->sin_addr.s_addr = host_to_network(address);
}

std::string Endpoint::Impl::address_string() const {
    if (m_type == UNDEFINED) {
        return "";
    }

    if (m_type == IP_V4) {
        const auto addr = reinterpret_cast<const ::sockaddr_in*>(&m_address_storage);
        return ip4_addr_to_string(network_to_host(addr->sin_addr.s_addr));
    }

    // Assuming IPv6
    char buf[INET6_ADDRSTRLEN];
    const auto addr = reinterpret_cast<const ::sockaddr_in6*>(&m_address_storage);
    Error convert_error = uv_inet_ntop(AF_INET6, &addr->sin6_addr, buf, INET6_ADDRSTRLEN);
    if (convert_error) {
        return "";
    }

    return buf;
}

std::uint16_t Endpoint::Impl::port() const {
    if (m_type == IP_V4) {
        const auto addr = reinterpret_cast<const ::sockaddr_in*>(&m_address_storage);
        return network_to_host(addr->sin_port);
    } else if (m_type == IP_V6) {
        const auto addr = reinterpret_cast<const ::sockaddr_in6*>(&m_address_storage);
        return network_to_host(addr->sin6_port);
    }

    return 0;
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

std::uint32_t Endpoint::Impl::ipv4_addr() const {
    if (m_type != IP_V4) {
        return 0;
    }

    const auto addr = reinterpret_cast<const ::sockaddr_in*>(&m_address_storage);
    return network_to_host(addr->sin_addr.s_addr);
}

void Endpoint::Impl::clear() {
    m_type = UNDEFINED;
    std::memset(&m_address_storage, 0, sizeof(::sockaddr_storage));
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TARM_IO_DEFINE_DEFAULT_MOVE(Endpoint);

Endpoint::Endpoint() :
    m_impl(new Impl()) {
}

Endpoint::Endpoint(const std::string& address, std::uint16_t port) :
    m_impl(new Impl(address, port)) {
}

Endpoint::Endpoint(std::uint32_t address, std::uint16_t port) :
    m_impl(new Impl(address, port)) {
}

Endpoint::Endpoint(unsigned long address, std::uint16_t port) :
    m_impl(new Impl(std::uint32_t(address), port)) {
}

Endpoint::Endpoint(const std::uint8_t* address_bytes, std::size_t address_size, std::uint16_t port) :
    m_impl(new Impl(address_bytes, address_size, port)) {
}

Endpoint::Endpoint(std::initializer_list<std::uint8_t> address_bytes, std::uint16_t port) :
    m_impl(new Impl(&*address_bytes.begin(), address_bytes.size(), port)) {
}

Endpoint::Endpoint(std::uint8_t (&address_bytes)[4], std::uint16_t port) :
    m_impl(new Impl(address_bytes, 4, port)) {
}

Endpoint::Endpoint(std::uint8_t (&address_bytes)[16], std::uint16_t port) :
    m_impl(new Impl(address_bytes, 16, port)) {
}

Endpoint::Endpoint(const sockaddr_placeholder* raw_address) :
    m_impl(new Impl(reinterpret_cast<const ::sockaddr*>(raw_address))) {
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

std::uint32_t Endpoint::ipv4_addr() const {
    return m_impl->ipv4_addr();
}

void Endpoint::clear() {
    return m_impl->clear();
}

///////////////////////////////////////// functions ///////////////////////////////////////////

std::ostream& operator <<(std::ostream& o, const Endpoint& e) {
    // see RFC 3986 (3.2.3) for details
    if (e.type() == Endpoint::IP_V4) {
        return o << e.address_string() << ":" << e.port();
    } else if (e.type() == Endpoint::IP_V6) {
        return o << "[" << e.address_string() << "]:" << e.port();
    }

    return o;
}

int raw_type(Endpoint::Type type) {
    switch (type) {
        case Endpoint::Type::IP_V4:
            return AF_INET;
        case Endpoint::Type::IP_V6:
            return AF_INET6;
        default:
            return AF_UNSPEC;
    }
}

} // namespace net
} // namespace io
} // namespace tarm

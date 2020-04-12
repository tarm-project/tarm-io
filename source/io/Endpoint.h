#pragma once

#include "CommonMacros.h"
#include "Export.h"
#include "Forward.h"

#include <array>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace io {

class Endpoint {
public:
    friend class TcpClient;
    friend class TcpServer;
    friend class UdpClient;
    friend class UdpPeer;
    friend class UdpServer;

    IO_DECLARE_DLL_PUBLIC_MOVE(Endpoint);;

    IO_DLL_PUBLIC Endpoint& operator=(const Endpoint& other);
    IO_DLL_PUBLIC Endpoint(const Endpoint& other);

    enum Type {
        UNDEFINED = 0,
        IP_V4,
        IP_V6
    };

    IO_DLL_PUBLIC Endpoint();
    IO_DLL_PUBLIC Endpoint(const std::string& address, std::uint16_t port);
    IO_DLL_PUBLIC Endpoint(std::uint32_t address, std::uint16_t port);
    IO_DLL_PUBLIC Endpoint(unsigned long address, std::uint16_t port);
    IO_DLL_PUBLIC Endpoint(const std::uint8_t* address_bytes, std::size_t address_size, std::uint16_t port);
    IO_DLL_PUBLIC Endpoint(std::initializer_list<std::uint8_t> address_bytes, std::uint16_t port);
    IO_DLL_PUBLIC Endpoint(std::uint8_t (&address_bytes)[4], std::uint16_t port);
    IO_DLL_PUBLIC Endpoint(std::uint8_t (&address_bytes)[16], std::uint16_t port);
    IO_DLL_PUBLIC Endpoint(const void* raw_address); // TODO: replace void* with cutom empty struct to prevent occasional pointer casts???
    IO_DLL_PUBLIC ~Endpoint();

    IO_DLL_PUBLIC std::string address_string() const;
    IO_DLL_PUBLIC std::uint16_t port() const;

    IO_DLL_PUBLIC Type type() const;

    // TODO: unit test this;
    IO_DLL_PUBLIC std::uint32_t ipv4_addr() const;

private:
    void* raw_endpoint();
    const void* raw_endpoint() const;

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

IO_DLL_PUBLIC
std::ostream& operator <<(std::ostream& o, const Endpoint& e);

} // namespace io

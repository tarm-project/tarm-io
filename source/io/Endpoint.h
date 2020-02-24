#pragma once

#include "CommonMacros.h"
#include "Export.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace io {

// TODO: private details in public domain
namespace detail {
    template<typename ParentType, typename ImplType>
    class UdpClientImplBase;
}


class Endpoint {
public:
    friend class TcpClient;
    friend class TcpServer;
    friend class UdpClient;
    friend class UdpPeer;
    friend class UdpServer;

    template<typename ParentType, typename ImplType>
    friend class detail::UdpClientImplBase;

    IO_ALLOW_MOVE(Endpoint);

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
    IO_DLL_PUBLIC Endpoint(const std::uint8_t* address_bytes, std::size_t address_size, std::uint16_t port);
    IO_DLL_PUBLIC Endpoint(std::initializer_list<std::uint8_t> address_bytes, std::uint16_t port);
    IO_DLL_PUBLIC ~Endpoint();

    IO_DLL_PUBLIC std::string address_string() const;
    IO_DLL_PUBLIC std::uint16_t port() const;

    IO_DLL_PUBLIC Type type() const;

private:
    void* raw_endpoint();
    const void* raw_endpoint() const;

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

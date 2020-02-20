#pragma once

#include "CommonMacros.h"
#include "Export.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace io {
/*
class Address {
public:
    Address(const std::string& str);
    Address(const std::string& str);



private:

};
*/

class Endpoint {
public:
    IO_FORBID_COPY(Endpoint);
    IO_ALLOW_MOVE(Endpoint);

    enum Type {
        UNDEFINED = 0,
        IP_V4,
        IP_V6
    };

    IO_DLL_PUBLIC Endpoint(const std::string& address, std::uint16_t port);
    IO_DLL_PUBLIC Endpoint(std::array<std::uint8_t, 4> address, std::uint16_t port);
    IO_DLL_PUBLIC Endpoint(std::array<std::uint8_t, 16> address, std::uint16_t port);
    IO_DLL_PUBLIC Endpoint(std::uint32_t address, std::uint16_t port);
    IO_DLL_PUBLIC ~Endpoint();

    IO_DLL_PUBLIC std::string as_string() const;
    IO_DLL_PUBLIC std::uint16_t port() const;

    IO_DLL_PUBLIC Type type() const;

    // TODO:
    IO_DLL_PUBLIC void* raw_endpoint();
    IO_DLL_PUBLIC const void* raw_endpoint() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

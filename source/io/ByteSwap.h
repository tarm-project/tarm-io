#pragma once

#include "Export.h"

#include <cstdint>

#include <type_traits>

#include <windows.h>

namespace io {

// network_to_host
template <typename T>
T network_to_host(T v) {
    static_assert(false, "For this type network_to_host is not supported.")
}

IO_DLL_PUBLIC
std::uint16_t network_to_host(std::uint16_t v);

IO_DLL_PUBLIC
std::uint32_t network_to_host(std::uint32_t v);

IO_DLL_PUBLIC
std::uint64_t network_to_host(std::uint64_t v);

// unsigned long is used to make MSVC happy
IO_DLL_PUBLIC
unsigned long network_to_host(unsigned long v);

// host_to_network
template <typename T>
T host_to_network(T v) {
    static_assert(false, "For this type host_to_network is not supported.")
}

IO_DLL_PUBLIC
std::uint16_t host_to_network(std::uint16_t v);

IO_DLL_PUBLIC
std::uint32_t host_to_network(std::uint32_t v);

IO_DLL_PUBLIC
std::uint64_t host_to_network(std::uint64_t v);

// unsigned long is used to make MSVC happy
IO_DLL_PUBLIC
unsigned long host_to_network(unsigned long v);

} // namespace io

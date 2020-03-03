#pragma once

#include "Export.h"

#include <cstdint>

#include <type_traits>

namespace io {

// network_to_host
template <typename T>
T network_to_host(T v);

IO_DLL_PUBLIC
std::uint16_t network_to_host(std::uint16_t v);

IO_DLL_PUBLIC
std::uint32_t network_to_host(std::uint32_t v);

IO_DLL_PUBLIC
std::uint64_t network_to_host(std::uint64_t v);

// TODO:

/*
// unsigned long is used to make MSVC happy
template <typename T>
typename std::enable_if<std::is_same<unsigned long, T>::value && sizeof(unsigned long) == 4, T>::type
network_to_host(T v) {
    return network_to_host(std::uint32_t(v));
}
*/

// host_to_network
template <typename T>
T host_to_network(T v);

IO_DLL_PUBLIC
std::uint16_t host_to_network(std::uint16_t v);

IO_DLL_PUBLIC
std::uint32_t host_to_network(std::uint32_t v);

IO_DLL_PUBLIC
std::uint64_t host_to_network(std::uint64_t v);

// TODO:
// unsigned long is used to make MSVC happy
template <typename T>
typename std::enable_if<std::is_same<unsigned long, T>::value && sizeof(unsigned long) == 4, T>::type
host_to_network(T v) {
    return host_to_network(std::uint32_t(v));
}

} // namespace io

#pragma once

#include <cstdint>

namespace io {

// TODO: unit test this

template <typename T>
T network_to_host(T v);
// TODO: verify MSVC
/*
#ifdef _WIN32
    { static_assert(false, "network_to_host with unknown type"); }
#else
    ;
#endif
*/

template <>
std::uint16_t network_to_host(std::uint16_t v);

template <>
std::int16_t network_to_host(std::int16_t v);

template <>
std::uint32_t network_to_host(std::uint32_t v);

template <> // unsigned long is used to make MSVC happy
unsigned long network_to_host(unsigned long v);

template <>
std::int32_t network_to_host(std::int32_t v);



template <typename T>
T host_to_network(T v);
// TODO: verify MSVC
/*
#ifdef _WIN32
{ static_assert(false, "host_to_network with unknown type"); }
#else
;
#endif
 */

} // namespace io

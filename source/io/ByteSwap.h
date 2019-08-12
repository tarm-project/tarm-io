#pragma once

#include <netinet/in.h>
#include <stdint.h>
/*
#include <endian.h>

#if __BYTE_ORDER == __BIG_ENDIAN
    #define ntohll(x) (x)
    #define htonll(x) (x)
#else
    #if __BYTE_ORDER == __LITTLE_ENDIAN
        #define ntohll(x) be64toh(x)
        #define htonll(x) htobe64(x)
    #endif
#endif
*/
namespace io {

template <typename T>
T network_to_host(T v);

template <>
inline uint16_t network_to_host(uint16_t v) { return ntohs(v); }

template <>
inline int16_t network_to_host(int16_t v) { return ntohs(v); }

template <>
inline uint32_t network_to_host(uint32_t v) { return ntohl(v); }

template <>
inline int32_t network_to_host(int32_t v) { return ntohl(v); }
/*
template <>
inline uint64_t network_to_host(uint64_t v) { return ntohll(v); }

template <>
inline int64_t network_to_host(int64_t v) { return ntohll(v); }
*/

template <typename T>
T host_to_network(T v);

template <>
inline uint16_t host_to_network(uint16_t v) { return htons(v); }

template <>
inline int16_t host_to_network(int16_t v) { return htons(v); }

template <>
inline uint32_t host_to_network(uint32_t v) { return htonl(v); }

template <>
inline int32_t host_to_network(int32_t v) { return htonl(v); }
/*
template <>
inline uint64_t host_to_network(uint64_t v) { return htonll(v); }

template <>
inline int64_t host_to_network(int64_t v) { return htonll(v); }
*/
} // namespace io

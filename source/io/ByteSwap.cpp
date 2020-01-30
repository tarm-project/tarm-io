#include "ByteSwap.h"

#ifdef _WIN32
    //#include <Winsock.h>
    #include <Winsock2.h>
#else
    #include <netinet/in.h>
#endif

#include <stdint.h>
#include <assert.h>
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

// network_to_host
template <>
std::uint16_t network_to_host(std::uint16_t v) { return ntohs(v); }

template <>
std::int16_t network_to_host(std::int16_t v) { return ntohs(v); }

template <>
std::uint32_t network_to_host(std::uint32_t v) { return ntohl(v); }

template <>
unsigned long network_to_host(unsigned long v) { return ntohl(v); }

template <>
std::int32_t network_to_host(std::int32_t v) { return ntohl(v); }

// host_to_network
template <>
std::uint16_t host_to_network(std::uint16_t v) { return htons(v); }

template <>
std::int16_t host_to_network(std::int16_t v) { return htons(v); }

template <>
std::uint32_t host_to_network(std::uint32_t v) { return htonl(v); }

template <>
unsigned long host_to_network(unsigned long v) { return htonl(v); }

template <>
std::int32_t host_to_network(std::int32_t v) { return htonl(v); }

} // namespace io

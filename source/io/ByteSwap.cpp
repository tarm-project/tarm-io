#include "ByteSwap.h"

#ifdef _WIN32
    #include <Winsock2.h>
    #include <stdlib.h>

    #if IO_BIG_ENDIAN
        #define IO_NTOHLL(x) (x)
        #define IO_HTONLL(x) (x)
    #else
        #define IO_NTOHLL(x) _byteswap_uint64(x)
        #define IO_HTONLL(x) _byteswap_uint64(x)
    #endif

#else
    #include <netinet/in.h>
    #ifdef __APPLE__
        #include <machine/endian.h>
    #else
        #include <endian.h>
    #endif

    #if __BYTE_ORDER == __BIG_ENDIAN
        #define IO_NTOHLL(x) (x)
        #define IO_HTONLL(x) (x)
    #else
        #if __BYTE_ORDER == __LITTLE_ENDIAN
            #define IO_NTOHLL(x) be64toh(x)
            #define IO_HTONLL(x) htobe64(x)
        #endif
    #endif
#endif

namespace io {

namespace {

// NetworkToHostDispatcher
template <typename T, std::size_t SIZE = sizeof(T)>
struct NetworkToHostDispatcher;

template <typename T>
struct NetworkToHostDispatcher<T, 4> {
    static T convert(T t) {
        return ntohl(t);
    }
};

template <typename T>
struct NetworkToHostDispatcher<T, 8> {
    static T convert(T t) {
        return IO_NTOHLL(v);
    }
};

// HostToNetworkDispatcher
template <typename T, std::size_t SIZE = sizeof(T)>
struct HostToNetworkDispatcher;

template <typename T>
struct HostToNetworkDispatcher<T, 4> {
    static T convert(T t) {
        return htonl(t);
    }
};

template <typename T>
struct HostToNetworkDispatcher<T, 8> {
    static T convert(T t) {
        return IO_HTONLL(v);
    }
};

} // namespace

// network_to_host
std::uint16_t network_to_host(std::uint16_t v) { return ntohs(v); }

std::uint32_t network_to_host(std::uint32_t v) { return ntohl(v); }

std::uint64_t network_to_host(std::uint64_t v) { return IO_NTOHLL(v); }

unsigned long network_to_host(unsigned long v) { return NetworkToHostDispatcher<unsigned long>::convert(v); }

// host_to_network
std::uint16_t host_to_network(std::uint16_t v) { return htons(v); }

std::uint32_t host_to_network(std::uint32_t v) { return htonl(v); }

std::uint64_t host_to_network(std::uint64_t v) { return IO_HTONLL(v); }

unsigned long host_to_network(unsigned long v) { return HostToNetworkDispatcher<unsigned long>::convert(v); }

} // namespace io

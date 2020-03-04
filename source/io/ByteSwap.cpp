#include "ByteSwap.h"

#include <cstring>

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
struct NetworkToHostDispatcher<T, 2> {
    static T convert(T t) {
        return ntohs(t);
    }
};

template <typename T>
struct NetworkToHostDispatcher<T, 4> {
    static T convert(T t) {
        return ntohl(t);
    }
};

template <typename T>
struct NetworkToHostDispatcher<T, 8> {
    static T convert(T t) {
        return IO_NTOHLL(t);
    }
};

// HostToNetworkDispatcher
template <typename T, std::size_t SIZE = sizeof(T)>
struct HostToNetworkDispatcher;

template <typename T>
struct HostToNetworkDispatcher<T, 2> {
    static T convert(T t) {
        return htons(t);
    }
};

template <typename T>
struct HostToNetworkDispatcher<T, 4> {
    static T convert(T t) {
        return htonl(t);
    }
};

template <typename T>
struct HostToNetworkDispatcher<T, 8> {
    static T convert(T t) {
        return IO_HTONLL(t);
    }
};

} // namespace

// network_to_host
unsigned short network_to_host(unsigned short v) { return NetworkToHostDispatcher<unsigned short>::convert(v); }

unsigned int network_to_host(unsigned int v) { return NetworkToHostDispatcher<unsigned int>::convert(v); }

unsigned long network_to_host(unsigned long v) { return NetworkToHostDispatcher<unsigned long>::convert(v); }

unsigned long long network_to_host(unsigned long long v) { return NetworkToHostDispatcher<unsigned long long>::convert(v); }

// host_to_network
unsigned short host_to_network(unsigned short v) { return HostToNetworkDispatcher<unsigned short>::convert(v); }

unsigned int host_to_network(unsigned int v) { return HostToNetworkDispatcher<unsigned int>::convert(v); }

unsigned long host_to_network(unsigned long v) { return HostToNetworkDispatcher<unsigned long>::convert(v); }

unsigned long long host_to_network(unsigned long long v) { return HostToNetworkDispatcher<unsigned long long>::convert(v); }

} // namespace io

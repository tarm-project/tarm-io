#include "ByteSwap.h"

#ifdef _WIN32
    #include <Winsock2.h>
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

// network_to_host
std::uint16_t network_to_host(std::uint16_t v) { return ntohs(v); }

std::uint32_t network_to_host(std::uint32_t v) { return ntohl(v); }

std::uint64_t network_to_host(std::uint64_t v) { return IO_NTOHLL(v); }

// host_to_network
std::uint16_t host_to_network(std::uint16_t v) { return htons(v); }

std::uint32_t host_to_network(std::uint32_t v) { return htonl(v); }

std::uint64_t host_to_network(std::uint64_t v) { return IO_HTONLL(v); }

} // namespace io

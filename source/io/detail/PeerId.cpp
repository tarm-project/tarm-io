#include "PeerId.h"

#include <uv.h>

namespace io {
namespace detail {

PeerId::PeerId(const void* address) {
    auto generic_address = reinterpret_cast<const ::sockaddr*>(address);

    if (generic_address->sa_family == AF_INET) {
        const auto& address = reinterpret_cast<const struct sockaddr_in*>(generic_address);
        address_low = address->sin_addr.s_addr;
        port = address->sin_port;
    } else {
        // TODO: ipv6
    }
}

} // namespace detail
} // namespace io

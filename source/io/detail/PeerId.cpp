/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

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
    } else if (generic_address->sa_family == AF_INET6) {
        const auto& address = reinterpret_cast<const struct sockaddr_in6*>(generic_address);
        address_high = *reinterpret_cast<const std::uint64_t*>(reinterpret_cast<const char*>(&address->sin6_addr));
        address_low = *reinterpret_cast<const std::uint64_t*>(reinterpret_cast<const char*>(&address->sin6_addr) + 8);
        port = address->sin6_port;
    }

    // else default empty values
}

} // namespace detail
} // namespace io

/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include <cstdint>
#include <functional>

namespace tarm {
namespace io {
namespace net {
namespace detail {

struct PeerId {
    PeerId(const void* address);

    std::uint64_t address_high = 0;
    std::uint64_t address_low = 0;
    std::uint16_t port = 0;
};

inline bool operator==(const PeerId& lhs, const PeerId& rhs) {
    return lhs.address_high == rhs.address_high &&
           lhs.address_low == rhs.address_low &&
           lhs.port == rhs.port;
}

} // namespace detail
} // namespace net
} // namespace io
} // namespace tarm

namespace std {
template <>
struct hash<::tarm::io::net::detail::PeerId> {
    std::size_t operator()(const ::tarm::io::net::detail::PeerId& id) const {
        return hash<std::uint64_t>()(id.address_high) ^
               hash<std::uint64_t>()(id.address_low) ^
               hash<std::uint16_t>()(id.port);
    }
};

} // namespace std

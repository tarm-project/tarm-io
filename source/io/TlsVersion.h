#pragma once

#include <cstdint>

namespace io {

// List all known versions of TLS. Some of them may be not available for some setups.
enum class TlsVersion : std::uint8_t {
    V1_0 = 0,
    V1_1,
    V1_2,
    V1_3,

    MIN = V1_0,
    MAX = V1_3
};

} // namespace io


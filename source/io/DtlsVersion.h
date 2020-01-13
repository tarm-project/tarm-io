#pragma once

#include <cstdint>

namespace io {

// List all known versions of DTLS. Some of them may be not available for some setups.
enum class DtlsVersion : std::uint8_t {
    UNKNOWN = 0,
    V1_0,
    V1_2,

    MIN = V1_0,
    MAX = V1_2
};

} // namespace io

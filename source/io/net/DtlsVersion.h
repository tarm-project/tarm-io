/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include <cstdint>
#include <tuple>

namespace tarm {
namespace io {
namespace net {

// List all known versions of DTLS. Some of them may be not available for some setups.
enum class DtlsVersion : std::uint8_t {
    UNKNOWN = 0,
    V1_0,
    V1_2,

    MIN = V1_0,
    MAX = V1_2
};

// Min and max version
using DtlsVersionRange = std::tuple<DtlsVersion, DtlsVersion>;

const DtlsVersionRange DEFAULT_DTLS_VERSION_RANGE{DtlsVersion::MIN, DtlsVersion::MAX};

} // namespace net
} // namespace io
} // namespace tarm

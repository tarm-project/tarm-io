/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "Export.h"

#include <cstdint>
#include <limits>

namespace tarm {
namespace io {
namespace detail {

/*
 * This class represents encoding and decoding of sizes in stream-oriented protocols like TCP.
 * It allows processing incomplete chunks of size bytes and have methods to query if value was completely decoded.
 * Maximum value of size is 2^56 - 1.
 */

// TODO: memory layout description

class VariableLengthSize {
public:
    TARM_IO_DLL_PUBLIC static constexpr std::uint64_t INVALID_VALUE = std::numeric_limits<std::uint64_t>::max();
    TARM_IO_DLL_PUBLIC static constexpr std::uint64_t MAX_VALUE = std::numeric_limits<std::uint64_t>::max() >> std::uint64_t(8);

    // Constructor for decoding
    TARM_IO_DLL_PUBLIC VariableLengthSize() = default;

    // Constructors for encoding
    TARM_IO_DLL_PUBLIC VariableLengthSize(std::uint8_t value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(std::uint16_t value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(std::uint32_t value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(std::uint64_t value);

    // Constructors for encoding, negative values are not supported
    TARM_IO_DLL_PUBLIC VariableLengthSize(std::int8_t value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(std::int16_t value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(std::int32_t value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(std::int64_t value);

    TARM_IO_DLL_PUBLIC std::uint64_t value() const;

    TARM_IO_DLL_PUBLIC bool is_complete() const;
    TARM_IO_DLL_PUBLIC std::size_t bytes_count() const;
    TARM_IO_DLL_PUBLIC const unsigned char* bytes() const;

    TARM_IO_DLL_PUBLIC bool add_byte(std::uint8_t b);
    // Returns bytes processed
    TARM_IO_DLL_PUBLIC std::size_t add_bytes(std::uint8_t* b, std::size_t count);

    TARM_IO_DLL_PUBLIC void reset();

private:
    void encode_impl(std::uint64_t v);
    void set_is_complete();

    static constexpr std::uint64_t IS_COMPLETE_MASK = std::uint64_t(1) << std::uint64_t(63);

    std::uint64_t m_encoded_value = 0;
    std::uint64_t m_decoded_value = 0;
};

} // namespace detail
} // namespace io
} // namespace tarm

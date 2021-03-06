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
namespace core {

/*
 * This class represents encoding and decoding of sizes in stream-oriented protocols like TCP.
 * It allows processing incomplete chunks of size bytes and have methods to query if value was completely decoded.
 * Maximum value of such size is 2^56 - 1.
 * Note: this is a bit similar to varint encoding from 'protobuf'
 *       https://developers.google.com/protocol-buffers/docs/encoding#varints
 * Note: the type is designed to have a balance between CPU time and memory consumption
 */

/*
In general memory layout looks in the following way:

 0                   1          ...        5                   6
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4  ...  7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ ... +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|M|  value bits |M|  value bits ...                   |M|  value bits |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ ... +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

So, up to 64 bits are used.
The fields have the following meaning:

marker (M): 1 bit
    If set to 1, then next byte is present. If set to 0, it is the last byte in a sequence.

value bits: 7 bits
    Chunk of resulting value, higher bits come first.

Examples.
 Value 1:                Value 128:
  0                       0                   1
  0 1 2 3 4 5 6 7         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
 +-+-+-+-+-+-+-+-+       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |0|0 0 0 0 0 0 1|       |1|0 0 0 0 0 0 1|0|0 0 0 0 0 0 0|
 +-+-+-+-+-+-+-+-+       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

class VariableLengthSize {
public:
    TARM_IO_DLL_PUBLIC static constexpr std::uint64_t INVALID_VALUE = std::numeric_limits<std::uint64_t>::max();
    TARM_IO_DLL_PUBLIC static constexpr std::uint64_t MAX_VALUE = std::numeric_limits<std::uint64_t>::max() >> std::uint64_t(8);

    // Constructor for decoding
    TARM_IO_DLL_PUBLIC VariableLengthSize() = default;

    // Constructors for encoding
    TARM_IO_DLL_PUBLIC VariableLengthSize(unsigned char value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(unsigned short value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(unsigned int value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(unsigned long value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(unsigned long long value);

    // Constructors for encoding, negative values are not supported
    TARM_IO_DLL_PUBLIC VariableLengthSize(char value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(signed char value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(short value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(int value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(long value);
    TARM_IO_DLL_PUBLIC VariableLengthSize(long long value);

    TARM_IO_DLL_PUBLIC std::uint64_t value() const;

    TARM_IO_DLL_PUBLIC bool is_complete() const;
    TARM_IO_DLL_PUBLIC std::size_t bytes_count() const;
    TARM_IO_DLL_PUBLIC const unsigned char* bytes() const;

    TARM_IO_DLL_PUBLIC bool add_byte(std::uint8_t b);
    // Returns bytes processed
    TARM_IO_DLL_PUBLIC std::size_t add_bytes(const std::uint8_t* b, std::size_t count);

    TARM_IO_DLL_PUBLIC void reset();

    TARM_IO_DLL_PUBLIC bool fail() const;

private:
    void encode_impl(std::uint64_t v);
    void set_is_complete();
    void set_is_fail();

    static constexpr std::uint64_t IS_COMPLETE_MASK = std::uint64_t(1) << std::uint64_t(63);
    static constexpr std::uint64_t IS_FAIL_MASK = std::uint64_t(1) << std::uint64_t(62);

    std::uint64_t m_encoded_value = 0;
    std::uint64_t m_decoded_value = 0;
};

} // namespace core
} // namespace io
} // namespace tarm

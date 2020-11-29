/*----------------------------------------------------------------------------------------------
*  Copyright (c) 2020 - present Alexander Voitenko
*  Licensed under the MIT License. See License.txt in the project root for license information.
*----------------------------------------------------------------------------------------------*/


#include "VariableLengthSize.h"

namespace tarm {
namespace io {
namespace detail {

// Definitions to make linker happy
constexpr std::uint64_t VariableLengthSize::INVALID_VALUE;
constexpr std::uint64_t VariableLengthSize::MAX_VALUE;

void VariableLengthSize::encode_impl(std::uint64_t v) {
    if (v & (~std::uint64_t(0x7F))) {
        std::uint64_t chunk = 0;
        std::uint64_t current_v = v;
        do {
            chunk = current_v & 0x7F;
            if (current_v != v) {
                m_encoded_value <<= 8;
                m_encoded_value |= chunk | 0x80;
            } else {
                m_encoded_value |= chunk;
            }

            current_v >>= 7;
        } while(current_v);
    } else {
        m_encoded_value = v;
    }
}

VariableLengthSize::VariableLengthSize(std::uint8_t value) {
    m_decoded_value = std::uint64_t(value) | IS_COMPLETE_MASK;
    encode_impl(value);
}

VariableLengthSize::VariableLengthSize(std::uint16_t value) {
    m_decoded_value = std::uint64_t(value) | IS_COMPLETE_MASK;
    encode_impl(value);
}

VariableLengthSize::VariableLengthSize(std::uint32_t value) {
    m_decoded_value = std::uint64_t(value) | IS_COMPLETE_MASK;
    encode_impl(value);
}

VariableLengthSize::VariableLengthSize(std::uint64_t value) {
    if (value <= MAX_VALUE) {
        m_decoded_value = std::uint64_t(value) | IS_COMPLETE_MASK;
        encode_impl(value);
    }
}

bool VariableLengthSize::is_complete() const {
    return m_decoded_value & IS_COMPLETE_MASK;
}

std::uint64_t VariableLengthSize::value() const {
    return is_complete() ? (m_decoded_value & (~IS_COMPLETE_MASK)) : INVALID_VALUE;
}

std::size_t VariableLengthSize::bytes_count() const {
    const auto all_m_bits_value = std::uint64_t(0x8080808080808080) & m_encoded_value;
    switch (all_m_bits_value) {
        case std::uint64_t(0):
            return is_complete() ? 1 : 0;
        case std::uint64_t(0x0000000000000080):
            return 2;
        case std::uint64_t(0x0000000000008080):
            return 3;
        case std::uint64_t(0x0000000000808080):
            return 4;
        case std::uint64_t(0x0000000080808080):
            return 5;
        case std::uint64_t(0x0000008080808080):
            return 6;
        case std::uint64_t(0x0000808080808080):
            return 7;
        case std::uint64_t(0x0080808080808080):
            return 8;
    };

    return 0;
}

void VariableLengthSize::reset() {
    m_encoded_value = 0;
    m_decoded_value = 0;
}

const unsigned char* VariableLengthSize::bytes() const {
    return reinterpret_cast<const unsigned char*>(&m_encoded_value);
}

} // namespace detail
} // namespace io
} // namespace tarm

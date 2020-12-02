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

VariableLengthSize::VariableLengthSize(std::int8_t value) :
   VariableLengthSize(std::int64_t(value)) {
}

VariableLengthSize::VariableLengthSize(std::int16_t value) :
    VariableLengthSize(std::int64_t(value)) {
}

VariableLengthSize::VariableLengthSize(std::int32_t value) :
    VariableLengthSize(std::int64_t(value)) {
}

VariableLengthSize::VariableLengthSize(std::int64_t value)  {
    if (value >= 0 && value <= MAX_VALUE) {
        m_decoded_value = std::uint64_t(value) | IS_COMPLETE_MASK;
        encode_impl(value);
    }
}


bool VariableLengthSize::is_complete() const {
    return m_decoded_value & IS_COMPLETE_MASK;
}

std::uint64_t VariableLengthSize::value() const {
    return is_complete() ? (m_decoded_value & std::uint64_t(0xFFFFFFFFFFFFFF)) : INVALID_VALUE;
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

bool VariableLengthSize::add_byte(std::uint8_t b) {
    if (is_complete()) {
        return false;
    }

    // As m_decoded_value has leading 8 bits free because of MAX_VALUE limitation,
    // utilizing 4 bit for storing bytes count during parsing process
    std::uint64_t bytes_count = (m_decoded_value & std::uint64_t(0xF00000000000000)) >> std::uint64_t(56);
    const bool should_stop = !(b & 0x80);
    if (bytes_count == 7 && !should_stop) {
        return false;
    }

    m_encoded_value >>= 8;
    m_encoded_value |= std::uint64_t(b) << 56;

    const std::uint64_t chunk = b & 0x7F;
    m_decoded_value <<= 7;
    m_decoded_value &= (~IS_COMPLETE_MASK);
    m_decoded_value |= (bytes_count + 1) << 56;
    m_decoded_value |= chunk;
    if (should_stop) {
        set_is_complete();
        m_encoded_value >>= 8 * (8 - bytes_count - 1);
    }

    return true;
}

std::size_t VariableLengthSize::add_bytes(std::uint8_t* b, std::size_t count) {
    if (b == nullptr) {
        return 0;
    }

    if (count == 0) {
        return 0;
    }

    std::size_t counter = 0;
    while(add_byte(b[counter])) {
        ++counter;
    }
    return counter;
}

void VariableLengthSize::set_is_complete()  {
    m_decoded_value |= IS_COMPLETE_MASK;
}

} // namespace detail
} // namespace io
} // namespace tarm

/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "core/VariableLengthSize.h"

struct VariableLengthSizeTest : public testing::Test,
                                public LogRedirector {

};

TEST_F(VariableLengthSizeTest, default_constructor) {
    io::core::VariableLengthSize v;
    EXPECT_FALSE(v.is_complete());
    EXPECT_EQ(io::core::VariableLengthSize::INVALID_VALUE, v.value());
    EXPECT_EQ(0, v.bytes_count());
}

TEST_F(VariableLengthSizeTest, encode_zero) {
    io::core::VariableLengthSize v(0u);
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(0, v.value());
    EXPECT_EQ(1, v.bytes_count());
    EXPECT_EQ(0, v.bytes()[0]);
}

TEST_F(VariableLengthSizeTest, encode_one) {
    io::core::VariableLengthSize v(1u);
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(1, v.value());
    EXPECT_EQ(1, v.bytes_count());
    EXPECT_EQ(1, v.bytes()[0]);
}

TEST_F(VariableLengthSizeTest, encode_127) {
    // Maximum value for 1 byte of raw data
    {
        io::core::VariableLengthSize v(std::uint8_t(127));
        EXPECT_TRUE(v.is_complete());
        EXPECT_EQ(127, v.value());
        EXPECT_EQ(1, v.bytes_count());
        EXPECT_EQ(127, v.bytes()[0]);
    }

    {
        io::core::VariableLengthSize v(std::uint16_t(127));
        EXPECT_TRUE(v.is_complete());
        EXPECT_EQ(127, v.value());
        EXPECT_EQ(1, v.bytes_count());
        EXPECT_EQ(127, v.bytes()[0]);
    }

    {
        io::core::VariableLengthSize v(std::uint32_t(127));
        EXPECT_TRUE(v.is_complete());
        EXPECT_EQ(127, v.value());
        EXPECT_EQ(1, v.bytes_count());
        EXPECT_EQ(127, v.bytes()[0]);
    }

    {
        io::core::VariableLengthSize v(std::uint64_t(127));
        EXPECT_TRUE(v.is_complete());
        EXPECT_EQ(127, v.value());
        EXPECT_EQ(1, v.bytes_count());
        EXPECT_EQ(127, v.bytes()[0]);
    }
}

TEST_F(VariableLengthSizeTest, encode_128) {
    {
        io::core::VariableLengthSize v(std::uint8_t(128));
        EXPECT_TRUE(v.is_complete());
        EXPECT_EQ(128, v.value());
        EXPECT_EQ(2, v.bytes_count());
        EXPECT_EQ(129, v.bytes()[0]);
        EXPECT_EQ(0, v.bytes()[1]);
    }

    {
        io::core::VariableLengthSize v(std::uint16_t(128));
        EXPECT_TRUE(v.is_complete());
        EXPECT_EQ(128, v.value());
        EXPECT_EQ(2, v.bytes_count());
        EXPECT_EQ(129, v.bytes()[0]);
        EXPECT_EQ(0, v.bytes()[1]);
    }

    {
        io::core::VariableLengthSize v(std::uint32_t(128));
        EXPECT_TRUE(v.is_complete());
        EXPECT_EQ(128, v.value());
        EXPECT_EQ(2, v.bytes_count());
        EXPECT_EQ(129, v.bytes()[0]);
        EXPECT_EQ(0, v.bytes()[1]);
    }

    {
        io::core::VariableLengthSize v(std::uint64_t(128));
        EXPECT_TRUE(v.is_complete());
        EXPECT_EQ(128, v.value());
        EXPECT_EQ(2, v.bytes_count());
        EXPECT_EQ(129, v.bytes()[0]);
        EXPECT_EQ(0, v.bytes()[1]);
    }
}

TEST_F(VariableLengthSizeTest, encode_255) {
    io::core::VariableLengthSize v(std::uint8_t(255));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(255, v.value());
    EXPECT_EQ(2, v.bytes_count());
    EXPECT_EQ(129, v.bytes()[0]);
    EXPECT_EQ(127, v.bytes()[1]);
}

TEST_F(VariableLengthSizeTest, encode_256) {
    io::core::VariableLengthSize v(std::uint16_t(256));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(256, v.value());
    EXPECT_EQ(2, v.bytes_count());
    EXPECT_EQ(130, v.bytes()[0]);
    EXPECT_EQ(0, v.bytes()[1]);
}

TEST_F(VariableLengthSizeTest, encode_x4000) {
    // Minimum value for 3 bytes of raw data
    io::core::VariableLengthSize v(std::uint16_t(0x4000));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(0x4000, v.value());
    EXPECT_EQ(3, v.bytes_count());
    EXPECT_EQ(129, v.bytes()[0]);
    EXPECT_EQ(128, v.bytes()[1]);
    EXPECT_EQ(0, v.bytes()[2]);
}

TEST_F(VariableLengthSizeTest, encode_65535) {
    // Max value of 2 bytes integer
    io::core::VariableLengthSize v(std::uint16_t(65535));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(65535, v.value());
    EXPECT_EQ(3, v.bytes_count());
    EXPECT_EQ(131, v.bytes()[0]);
    EXPECT_EQ(255, v.bytes()[1]);
    EXPECT_EQ(127, v.bytes()[2]);
}

TEST_F(VariableLengthSizeTest, encode_x1FFFFF) {
    // Maximum value for 3 bytes of raw data
    io::core::VariableLengthSize v(std::uint32_t(0x1FFFFF));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(0x1FFFFF, v.value());
    EXPECT_EQ(3, v.bytes_count());
    EXPECT_EQ(255, v.bytes()[0]);
    EXPECT_EQ(255, v.bytes()[1]);
    EXPECT_EQ(127, v.bytes()[2]);
}

TEST_F(VariableLengthSizeTest, encode_x200000) {
    // Minimal value for 4 bytes of raw data
    io::core::VariableLengthSize v(std::uint32_t(0x200000));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(0x200000, v.value());
    EXPECT_EQ(4, v.bytes_count());
    EXPECT_EQ(129, v.bytes()[0]);
    EXPECT_EQ(128, v.bytes()[1]);
    EXPECT_EQ(128, v.bytes()[2]);
    EXPECT_EQ(0, v.bytes()[3]);
}

TEST_F(VariableLengthSizeTest, encode_xFFFFFFF) {
    // Maximum value for 4 bytes of raw data
    io::core::VariableLengthSize v(std::uint32_t(0xFFFFFFF));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(0xFFFFFFF, v.value());
    EXPECT_EQ(4, v.bytes_count());
    EXPECT_EQ(255, v.bytes()[0]);
    EXPECT_EQ(255, v.bytes()[1]);
    EXPECT_EQ(255, v.bytes()[2]);
    EXPECT_EQ(127, v.bytes()[3]);
}

TEST_F(VariableLengthSizeTest, encode_x10000000) {
    // Minimal value for 5 bytes of raw data
    io::core::VariableLengthSize v(std::uint32_t(0x10000000));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(0x10000000, v.value());
    EXPECT_EQ(5, v.bytes_count());
    EXPECT_EQ(129, v.bytes()[0]);
    EXPECT_EQ(128, v.bytes()[1]);
    EXPECT_EQ(128, v.bytes()[2]);
    EXPECT_EQ(128, v.bytes()[3]);
    EXPECT_EQ(0, v.bytes()[4]);
}

TEST_F(VariableLengthSizeTest, encode_xFFFFFFFF) {
    // Maximum uint32 value
    io::core::VariableLengthSize v(std::uint32_t(0xFFFFFFFF));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(0xFFFFFFFF, v.value());
    EXPECT_EQ(5, v.bytes_count());
    EXPECT_EQ(143, v.bytes()[0]);
    EXPECT_EQ(255, v.bytes()[1]);
    EXPECT_EQ(255, v.bytes()[2]);
    EXPECT_EQ(255, v.bytes()[3]);
    EXPECT_EQ(127, v.bytes()[4]);
}

TEST_F(VariableLengthSizeTest, encode_x100000000) {
    // The first value which can be represented as uint64 only
    io::core::VariableLengthSize v(std::uint64_t(0x100000000));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(0x100000000, v.value());
    EXPECT_EQ(5, v.bytes_count());
    EXPECT_EQ(144, v.bytes()[0]);
    EXPECT_EQ(128, v.bytes()[1]);
    EXPECT_EQ(128, v.bytes()[2]);
    EXPECT_EQ(128, v.bytes()[3]);
    EXPECT_EQ(0, v.bytes()[4]);
}

TEST_F(VariableLengthSizeTest, encode_max_possible_value) {
    // 0xFFFFFFFFFFFFFF
    io::core::VariableLengthSize v(std::uint64_t(0xFFFFFFFFFFFFFF));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(0xFFFFFFFFFFFFFF, v.value());
    EXPECT_EQ(8, v.bytes_count());
    EXPECT_EQ(255, v.bytes()[0]);
    EXPECT_EQ(255, v.bytes()[1]);
    EXPECT_EQ(255, v.bytes()[2]);
    EXPECT_EQ(255, v.bytes()[3]);
    EXPECT_EQ(255, v.bytes()[4]);
    EXPECT_EQ(255, v.bytes()[5]);
    EXPECT_EQ(255, v.bytes()[6]);
    EXPECT_EQ(127, v.bytes()[7]);
}

TEST_F(VariableLengthSizeTest, encode_xA5A5A5A5A5A5A) {
    // 1010 0101... bit pattern
    io::core::VariableLengthSize v(std::uint64_t(0xA5A5A5A5A5A5A));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(0xA5A5A5A5A5A5A, v.value());
    EXPECT_EQ(8, v.bytes_count());
    EXPECT_EQ(133, v.bytes()[0]);
    EXPECT_EQ(150, v.bytes()[1]);
    EXPECT_EQ(203, v.bytes()[2]);
    EXPECT_EQ(165, v.bytes()[3]);
    EXPECT_EQ(210, v.bytes()[4]);
    EXPECT_EQ(233, v.bytes()[5]);
    EXPECT_EQ(180, v.bytes()[6]);
    EXPECT_EQ(90, v.bytes()[7]);
}

TEST_F(VariableLengthSizeTest, value_bigger_than_supported) {
    io::core::VariableLengthSize v(std::uint64_t(0xFFFFFFFFFFFFFFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_EQ(io::core::VariableLengthSize::INVALID_VALUE, v.value());
    EXPECT_EQ(0, v.bytes_count());
}

TEST_F(VariableLengthSizeTest, encode_signed_integers) {
    {
        io::core::VariableLengthSize ok_v(std::int8_t(1));
        EXPECT_TRUE(ok_v.is_complete());
        EXPECT_EQ(1, ok_v.value());

        io::core::VariableLengthSize error_v(std::int8_t(-1));
        EXPECT_FALSE(error_v.is_complete());
        EXPECT_EQ(io::core::VariableLengthSize::INVALID_VALUE, error_v.value());
    }

    {
        io::core::VariableLengthSize ok_v(std::int16_t(1));
        EXPECT_TRUE(ok_v.is_complete());
        EXPECT_EQ(1, ok_v.value());

        io::core::VariableLengthSize error_v(std::int16_t(-1));
        EXPECT_FALSE(error_v.is_complete());
        EXPECT_EQ(io::core::VariableLengthSize::INVALID_VALUE, error_v.value());
    }

    {
        io::core::VariableLengthSize ok_v(std::int32_t(1));
        EXPECT_TRUE(ok_v.is_complete());
        EXPECT_EQ(1, ok_v.value());

        io::core::VariableLengthSize error_v(std::int32_t(-1));
        EXPECT_FALSE(error_v.is_complete());
        EXPECT_EQ(io::core::VariableLengthSize::INVALID_VALUE, error_v.value());
    }

    {
        io::core::VariableLengthSize ok_v(std::int64_t(1));
        EXPECT_TRUE(ok_v.is_complete());
        EXPECT_EQ(1, ok_v.value());

        io::core::VariableLengthSize error_v(std::int64_t(-1));
        EXPECT_FALSE(error_v.is_complete());
        EXPECT_EQ(io::core::VariableLengthSize::INVALID_VALUE, error_v.value());
    }
}

TEST_F(VariableLengthSizeTest, decode_zero) {
    io::core::VariableLengthSize v;
    EXPECT_TRUE(v.add_byte(0));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(0, v.value());
    EXPECT_EQ(1, v.bytes_count());
    EXPECT_EQ(0, v.bytes()[0]);

    EXPECT_FALSE(v.add_byte(0));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(0, v.value());
    EXPECT_EQ(1, v.bytes_count());
    EXPECT_EQ(0, v.bytes()[0]);
}

TEST_F(VariableLengthSizeTest, decode_1) {
    io::core::VariableLengthSize v;
    EXPECT_TRUE(v.add_byte(1));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(1, v.value());
    EXPECT_EQ(1, v.bytes_count());
    EXPECT_EQ(1, v.bytes()[0]);

    EXPECT_FALSE(v.add_byte(1));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(1, v.value());
    EXPECT_EQ(1, v.bytes_count());
    EXPECT_EQ(1, v.bytes()[0]);
}

TEST_F(VariableLengthSizeTest, decode_127) {
    // Maximum 1-byte value
    io::core::VariableLengthSize v;
    EXPECT_TRUE(v.add_byte(127));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(127, v.value());
    EXPECT_EQ(1, v.bytes_count());
    EXPECT_EQ(127, v.bytes()[0]);

    EXPECT_FALSE(v.add_byte(100)); // different value just for fun
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(127, v.value());
    EXPECT_EQ(1, v.bytes_count());
    EXPECT_EQ(127, v.bytes()[0]);
}

TEST_F(VariableLengthSizeTest, decode_128) {
    io::core::VariableLengthSize v;
    EXPECT_TRUE(v.add_byte(129));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(128, v.value());
    EXPECT_EQ(2, v.bytes_count());
    EXPECT_EQ(129, v.bytes()[0]);
    EXPECT_EQ(0, v.bytes()[1]);
}

TEST_F(VariableLengthSizeTest, decode_255) {
    io::core::VariableLengthSize v;
    EXPECT_TRUE(v.add_byte(129));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(127));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(255, v.value());
    EXPECT_EQ(2, v.bytes_count());
    EXPECT_EQ(129, v.bytes()[0]);
    EXPECT_EQ(127, v.bytes()[1]);
}

TEST_F(VariableLengthSizeTest, decode_with_leading_zeroes) {
    // Leading zeroes do not have practical meaning but allowed by protocol
    io::core::VariableLengthSize v;
    EXPECT_TRUE(v.add_byte(0x80));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0x80));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0x80));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0x01));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(1, v.value());
    EXPECT_EQ(4, v.bytes_count());
    EXPECT_EQ(0x80, v.bytes()[0]);
    EXPECT_EQ(0x80, v.bytes()[1]);
    EXPECT_EQ(0x80, v.bytes()[2]);
    EXPECT_EQ(0x01, v.bytes()[3]);
}

TEST_F(VariableLengthSizeTest, decode_max_value) {
    io::core::VariableLengthSize v;
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0x7F));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(std::uint64_t(0xFFFFFFFFFFFFFF), v.value());
    EXPECT_EQ(8, v.bytes_count());
    EXPECT_EQ(0xFF, v.bytes()[0]);
    EXPECT_EQ(0xFF, v.bytes()[1]);
    EXPECT_EQ(0xFF, v.bytes()[2]);
    EXPECT_EQ(0xFF, v.bytes()[3]);
    EXPECT_EQ(0xFF, v.bytes()[4]);
    EXPECT_EQ(0xFF, v.bytes()[5]);
    EXPECT_EQ(0xFF, v.bytes()[6]);
    EXPECT_EQ(0x7F, v.bytes()[7]);
}

TEST_F(VariableLengthSizeTest, to_many_bytes) {
    io::core::VariableLengthSize v;
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_TRUE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    // according to message format this should be the last byte, but we try to continue
    EXPECT_FALSE(v.add_byte(0xFF));
    EXPECT_FALSE(v.is_complete());
    EXPECT_EQ(io::core::VariableLengthSize::INVALID_VALUE, v.value());
}

TEST_F(VariableLengthSizeTest, reset_1)  {
    io::core::VariableLengthSize v;
    v.reset();
    EXPECT_FALSE(v.is_complete());
    EXPECT_EQ(io::core::VariableLengthSize::INVALID_VALUE, v.value());
    EXPECT_EQ(0, v.bytes_count());
    EXPECT_TRUE(v.add_byte(0));
    v.reset();
    EXPECT_FALSE(v.is_complete());
    EXPECT_EQ(io::core::VariableLengthSize::INVALID_VALUE, v.value());
    EXPECT_EQ(0, v.bytes_count());
}

TEST_F(VariableLengthSizeTest, reset_2)  {
    io::core::VariableLengthSize v(100500);
    v.reset();
    EXPECT_FALSE(v.is_complete());
    EXPECT_EQ(io::core::VariableLengthSize::INVALID_VALUE, v.value());
    EXPECT_EQ(0, v.bytes_count());
}

TEST_F(VariableLengthSizeTest, copy_constructor) {
    io::core::VariableLengthSize v_1(100500);
    io::core::VariableLengthSize v_2 = v_1;
    EXPECT_TRUE(v_2.is_complete());
    EXPECT_EQ(100500, v_2.value());
}

TEST_F(VariableLengthSizeTest, copy_assignment) {
    io::core::VariableLengthSize v_1(100500);
    io::core::VariableLengthSize v_2;
    v_2 = v_1;
    EXPECT_TRUE(v_2.is_complete());
    EXPECT_EQ(100500, v_2.value());
}

TEST_F(VariableLengthSizeTest, add_bytes_invalid_values) {
    io::core::VariableLengthSize v;
    EXPECT_EQ(0, v.add_bytes(nullptr, 10));
    unsigned char byte = 0;
    EXPECT_EQ(0, v.add_bytes(&byte, 0));
}

TEST_F(VariableLengthSizeTest, add_bytes) {
    io::core::VariableLengthSize v;
    unsigned char buf[] = {
        0x80, 0, 0x80, 1, 0x80, 2 // 0, 1, 2 (2  bytes each)
    };

    ASSERT_EQ(2, v.add_bytes(buf, 6));
    EXPECT_EQ(0, v.value());

    v.reset();

    ASSERT_EQ(2, v.add_bytes(buf + 2, 4));
    EXPECT_EQ(1, v.value());

    v.reset();

    ASSERT_EQ(2, v.add_bytes(buf + 4, 2));
    EXPECT_EQ(2, v.value());
}

TEST_F(VariableLengthSizeTest, add_bytes_in_chunks) {
    io::core::VariableLengthSize v;
    unsigned char buf[] = {
        0x80, 0x80, 0x80, 0x0
    };

    ASSERT_EQ(1, v.add_bytes(buf, 1));
    EXPECT_FALSE(v.is_complete());

    ASSERT_EQ(2, v.add_bytes(buf + 1, 2));
    EXPECT_FALSE(v.is_complete());

    ASSERT_EQ(1, v.add_bytes(buf + 3, 1));
    EXPECT_TRUE(v.is_complete());
    EXPECT_EQ(0, v.value());
}

/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "Convert.h"

struct ConvertTest : public testing::Test,
                     public LogRedirector {
};

TEST_F(ConvertTest, ip4_addr_to_string_single_conversion) {
    EXPECT_EQ("255.127.7.1", io::ip4_addr_to_string(0xFF7F0701));
}

TEST_F(ConvertTest, ip4_addr_to_string_batch_conversion) {
    // Test description: just more complicated test to cover more values and cases
    const std::uint32_t SIZE = 200000;

    for (std::uint32_t i = 0; i < SIZE; ++i) {
        const std::uint32_t addr = i * 2000000 + i;

        const std::uint8_t addr_1 = (addr & 0xFF000000u) >> 24;
        const std::uint8_t addr_2 = (addr & 0x00FF0000u) >> 16;
        const std::uint8_t addr_3 = (addr & 0x0000FF00u) >> 8;
        const std::uint8_t addr_4 = (addr & 0x000000FFu);

        std::string expected;
        expected += std::to_string(addr_1);
        expected += ".";
        expected += std::to_string(addr_2);
        expected += ".";
        expected += std::to_string(addr_3);
        expected += ".";
        expected += std::to_string(addr_4);

        std::string actual = io::ip4_addr_to_string(addr);
        ASSERT_EQ(expected, actual) << " i= " << i;
    }
}

TEST_F(ConvertTest, string_to_ip4_addr) {
    std::uint32_t address = 0;
    EXPECT_FALSE(io::string_to_ip4_addr("127.0.0.1", address));
    EXPECT_EQ(0x7F000001, address);
}

TEST_F(ConvertTest, string_to_ip4_addr_invalid_value) {
    std::uint32_t address = 0;
    auto error = io::string_to_ip4_addr("127.0.", address);
    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
}

TEST_F(ConvertTest, string_to_ip4_addr_empty) {
    std::uint32_t address = 0;
    auto error = io::string_to_ip4_addr("", address);
    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
}

TEST_F(ConvertTest, ip6_addr_to_string_nullptr) {
    const auto string = io::ip6_addr_to_string(nullptr);
    EXPECT_TRUE(string.empty());
}

TEST_F(ConvertTest, ip6_addr_to_string_single_conversion) {
    const auto string = io::ip6_addr_to_string({0, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1});
    EXPECT_EQ("7f::1", string);
}

TEST_F(ConvertTest, string_to_ip6_addr_invalid) {
    std::array<std::uint8_t, 16> arr;
    auto error = io::string_to_ip6_addr("10.100.11.3", arr);
    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
}

TEST_F(ConvertTest, string_to_ip6_addr_single) {
    std::array<std::uint8_t, 16> arr = {};
    auto error = io::string_to_ip6_addr("AABB::ccdd", arr);
    EXPECT_FALSE(error);
    EXPECT_EQ(0xAA, arr[0]);
    EXPECT_EQ(0xBB, arr[1]);
    EXPECT_EQ(0x00, arr[2]);
    EXPECT_EQ(0x00, arr[3]);
    EXPECT_EQ(0x00, arr[4]);
    EXPECT_EQ(0x00, arr[5]);
    EXPECT_EQ(0x00, arr[6]);
    EXPECT_EQ(0x00, arr[7]);
    EXPECT_EQ(0x00, arr[8]);
    EXPECT_EQ(0x00, arr[9]);
    EXPECT_EQ(0x00, arr[10]);
    EXPECT_EQ(0x00, arr[11]);
    EXPECT_EQ(0x00, arr[12]);
    EXPECT_EQ(0x00, arr[13]);
    EXPECT_EQ(0xCC, arr[14]);
    EXPECT_EQ(0xDD, arr[15]);
}

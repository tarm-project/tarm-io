#include "UTCommon.h"

#include "io/Convert.h"

struct CommonTest : public testing::Test,
                    public LogRedirector {
};

TEST_F(CommonTest, ip4_addr_single_conversion) {
    EXPECT_EQ("255.127.7.1", io::ip4_addr_to_string(0xFF7F0701));
}

TEST_F(CommonTest, ip4_addr_batch_conversion) {
    // Test description: just more complicated test to cover more values and cases
    const std::size_t SIZE = 2000;

    for (std::size_t i = 0; i < SIZE; ++i) {
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

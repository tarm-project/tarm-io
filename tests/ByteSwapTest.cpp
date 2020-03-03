#include "UTCommon.h"

#include "io/ByteSwap.h"

struct ByteSwapTest : public testing::Test,
                      public LogRedirector {
};

TEST_F(ByteSwapTest, network_to_host) {
    EXPECT_EQ(std::uint16_t(0x0201),                io::network_to_host(std::uint16_t(0x0102)));
    EXPECT_EQ(std::uint32_t(0x04030201),            io::network_to_host(std::uint32_t(0x01020304)));
    EXPECT_EQ(std::uint64_t(0x0807060504030201ull), io::network_to_host(std::uint64_t(0x0102030405060708ull)));

    if (sizeof(unsigned long) == 4) {
        EXPECT_EQ(unsigned long(0x04030201), io::network_to_host(unsigned long(0x01020304)));
    } else {
        EXPECT_EQ(unsigned long(0x0807060504030201ull), io::network_to_host(unsigned long(0x0102030405060708ull)));
    }
}

TEST_F(ByteSwapTest, host_to_network) {
    EXPECT_EQ(std::uint16_t(0x0201),                io::host_to_network(std::uint16_t(0x0102)));
    EXPECT_EQ(std::uint32_t(0x04030201),            io::host_to_network(std::uint32_t(0x01020304)));
    EXPECT_EQ(std::uint64_t(0x0807060504030201ull), io::host_to_network(std::uint64_t(0x0102030405060708ull)));

    if (sizeof(unsigned long) == 4) {
        EXPECT_EQ(unsigned long(0x04030201), io::host_to_network(unsigned long(0x01020304)));
    } else {
        EXPECT_EQ(unsigned long(0x0807060504030201ull), io::host_to_network(unsigned long(0x0102030405060708ull)));
    }
}
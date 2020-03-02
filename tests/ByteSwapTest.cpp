#include "UTCommon.h"

#include "io/ByteSwap.h"

struct ByteSwapTest : public testing::Test,
                     public LogRedirector {
};

TEST_F(ByteSwapTest, network_to_host_uint16_t) {
    //

    io::network_to_host(unsigned(0xFF));
    io::network_to_host(std::uint16_t(0xFF));
    io::network_to_host(std::uint32_t(0xFF));
    io::network_to_host(std::uint64_t(0xFF));
}
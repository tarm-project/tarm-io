#include "UTCommon.h"

#include "io/Endpoint.h"

struct EndpointTest : public testing::Test,
                      public LogRedirector {
};

TEST_F(EndpointTest, default_constructor) {
    io::Endpoint endpoint;
    EXPECT_EQ(io::Endpoint::UNDEFINED, endpoint.type());
}

TEST_F(EndpointTest, ipv4_from_array) {
    std::uint8_t array[4] = {127, 0, 0, 1};
    io::Endpoint endpoint(array, sizeof(array), 0);
    EXPECT_EQ(io::Endpoint::IP_V4, endpoint.type());
    EXPECT_EQ("127.0.0.1", endpoint.address_string());
}
#include "UTCommon.h"

#include "io/Endpoint.h"

struct EndpointTest : public testing::Test,
                      public LogRedirector {
};

TEST_F(EndpointTest, default_constructor) {
    io::Endpoint endpoint;
    EXPECT_EQ(io::Endpoint::UNDEFINED, endpoint.type());
}

TEST_F(EndpointTest, ipv4_from_raw_bytes) {
    std::uint8_t array[4] = {127, 0, 0, 1};
    io::Endpoint endpoint(array, sizeof(array), 0);
    EXPECT_EQ(io::Endpoint::IP_V4, endpoint.type());
    EXPECT_EQ("127.0.0.1", endpoint.address_string());
}

TEST_F(EndpointTest, ipv4_from_array) {
    std::uint8_t array[4] = {127, 0, 0, 1};
    io::Endpoint endpoint(array, 0);
    EXPECT_EQ(io::Endpoint::IP_V4, endpoint.type());
    EXPECT_EQ("127.0.0.1", endpoint.address_string());
}

TEST_F(EndpointTest, copy_constructor) {
    std::uint8_t array[4] = {127, 0, 0, 1};
    io::Endpoint endpoint(array, sizeof(array), 1234);

    io::Endpoint endpoint_copy(endpoint);

    EXPECT_EQ(io::Endpoint::IP_V4, endpoint_copy.type());
    EXPECT_EQ("127.0.0.1", endpoint_copy.address_string());
    EXPECT_EQ(1234, endpoint_copy.port());
}

TEST_F(EndpointTest, copy_assignment) {
    std::uint8_t array[4] = {127, 0, 0, 1};
    io::Endpoint endpoint(array, sizeof(array), 1234);

    io::Endpoint endpoint_copy;
    endpoint_copy = endpoint;

    EXPECT_EQ(io::Endpoint::IP_V4, endpoint_copy.type());
    EXPECT_EQ("127.0.0.1", endpoint_copy.address_string());
    EXPECT_EQ(1234, endpoint_copy.port());
}

TEST_F(EndpointTest, inplace_address) {
    io::Endpoint endpoint({127, 0, 0, 1}, 1234);

    EXPECT_EQ(io::Endpoint::IP_V4, endpoint.type());
    EXPECT_EQ("127.0.0.1", endpoint.address_string());
    EXPECT_EQ(1234, endpoint.port());
}

TEST_F(EndpointTest, empty_inplace_address) {
    io::Endpoint endpoint({}, 1234);

    EXPECT_EQ(io::Endpoint::UNDEFINED, endpoint.type());
    EXPECT_EQ("", endpoint.address_string());
    EXPECT_EQ(0, endpoint.port());
}

TEST_F(EndpointTest, ipv6_from_string) {
    io::Endpoint endpoint("::1", 1234);

    EXPECT_EQ(io::Endpoint::IP_V6, endpoint.type());
    EXPECT_EQ("::1", endpoint.address_string());
    EXPECT_EQ(1234, endpoint.port());
}

TEST_F(EndpointTest, ipv6_init_from_array) {
    std::uint8_t array[16] = {0, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    io::Endpoint endpoint(array, sizeof(array), 1234);
    EXPECT_EQ("7f::1", endpoint.address_string());
}

TEST_F(EndpointTest, ipv6_init_from_initializer_list) {
    io::Endpoint endpoint({0, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, 1234);
    EXPECT_EQ("7f::1", endpoint.address_string());
}


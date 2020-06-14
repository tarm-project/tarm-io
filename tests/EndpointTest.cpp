/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "io/ByteSwap.h"
#include "io/Endpoint.h"

#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    #include <sys/socket.h>
    #include <netinet/in.h>
#endif

#include <sstream>

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
    EXPECT_EQ(0x7F000001, endpoint.ipv4_addr());
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

TEST_F(EndpointTest, ostream_1) {
    io::Endpoint endpoint;
    std::ostringstream oss;
    oss << endpoint;
    EXPECT_EQ("", oss.str());
}

TEST_F(EndpointTest, ostream_2) {
    io::Endpoint endpoint({127, 2, 2, 1}, 1234);
    std::ostringstream oss;
    oss << endpoint;
    EXPECT_EQ("127.2.2.1:1234", oss.str());
}

TEST_F(EndpointTest, ostream_3) {
    io::Endpoint endpoint({0, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, 1234);
    std::ostringstream oss;
    oss << endpoint;
    EXPECT_EQ("[7f::1]:1234", oss.str());
}

#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
// TODO: this test on Windows??????? (include <uv.h> for system specific headers)
TEST_F(EndpointTest, ipv4_init_from_raw_sockaddr) {
    ::sockaddr_in ipv4_sockaddr;
    std::memset(&ipv4_sockaddr, 0, sizeof(ipv4_sockaddr));

    ipv4_sockaddr.sin_family = AF_INET;
    ipv4_sockaddr.sin_port = io::host_to_network(std::uint16_t(1500));
    ipv4_sockaddr.sin_addr.s_addr = io::host_to_network(std::uint32_t(0x7F000001));

    io::Endpoint endpoint(reinterpret_cast<const io::Endpoint::sockaddr_placeholder*>(&ipv4_sockaddr));
    EXPECT_EQ(io::Endpoint::IP_V4, endpoint.type());
    EXPECT_EQ(1500, endpoint.port());
    EXPECT_EQ("127.0.0.1", endpoint.address_string());
}

TEST_F(EndpointTest, ipv6_init_from_raw_sockaddr) {
    ::sockaddr_in6 ipv6_sockaddr;
    std::memset(&ipv6_sockaddr, 0, sizeof(ipv6_sockaddr));

    ipv6_sockaddr.sin6_family = AF_INET6;
    ipv6_sockaddr.sin6_port = io::host_to_network(std::uint16_t(1500));
    const unsigned char buf[16] = {0, 0x7f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10};
    std::memcpy(&ipv6_sockaddr.sin6_addr, buf, 16);

    io::Endpoint endpoint(reinterpret_cast<const io::Endpoint::sockaddr_placeholder*>(&ipv6_sockaddr));
    EXPECT_EQ(io::Endpoint::IP_V6, endpoint.type());
    EXPECT_EQ(1500, endpoint.port());
    EXPECT_EQ("7f::10", endpoint.address_string());
}
#endif

TEST_F(EndpointTest, clear_ipv4) {
    io::Endpoint endpoint({127, 2, 2, 1}, 1234);
    endpoint.clear();
    EXPECT_EQ(io::Endpoint::UNDEFINED, endpoint.type());
}

TEST_F(EndpointTest, clear_ipv6) {
    io::Endpoint endpoint({0, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, 1234);
    endpoint.clear();
    EXPECT_EQ(io::Endpoint::UNDEFINED, endpoint.type());
}

/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "net/Dns.h"

struct DnsTest : public testing::Test,
                 public LogRedirector {
};

TEST_F(DnsTest, look_up_any) {
    std::size_t on_resolve_callback_count = 0;

    io::EventLoop loop;
    io::net::resolve_host(loop, "google.com",
        [&](const std::vector<io::net::Endpoint>& endpoints, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++on_resolve_callback_count;
            ASSERT_FALSE(endpoints.empty());
            EXPECT_NE(io::net::Endpoint::UNDEFINED, endpoints.front().type());
        }
    );

    EXPECT_EQ(0, on_resolve_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_resolve_callback_count);
}

TEST_F(DnsTest, look_up_ipv4) {
    std::size_t on_resolve_callback_count = 0;

    io::EventLoop loop;
    io::net::resolve_host(loop, "google.com",
        [&](const std::vector<io::net::Endpoint>& endpoints, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++on_resolve_callback_count;
            ASSERT_FALSE(endpoints.empty());
            EXPECT_EQ(io::net::Endpoint::IP_V4, endpoints.front().type());
        },
        io::net::Endpoint::IP_V4
    );

    EXPECT_EQ(0, on_resolve_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_resolve_callback_count);
}

TEST_F(DnsTest, look_up_ipv6) {
    std::size_t on_resolve_callback_count = 0;

    io::EventLoop loop;
    io::net::resolve_host(loop, "google.com",
        [&](const std::vector<io::net::Endpoint>& endpoints, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++on_resolve_callback_count;
            ASSERT_FALSE(endpoints.empty());
            EXPECT_EQ(io::net::Endpoint::IP_V6, endpoints.front().type());
        },
        io::net::Endpoint::IP_V6
    );

    EXPECT_EQ(0, on_resolve_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_resolve_callback_count);
}

TEST_F(DnsTest, look_up_unknown) {
    std::size_t on_resolve_callback_count = 0;

    io::EventLoop loop;
    io::net::resolve_host(loop, "!!!",
        [&](const std::vector<io::net::Endpoint>& endpoints, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::UNKNOWN_NODE_OR_SERVICE, error.code());
            ++on_resolve_callback_count;
            EXPECT_TRUE(endpoints.empty());
        }
    );

    EXPECT_EQ(0, on_resolve_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_resolve_callback_count);
}

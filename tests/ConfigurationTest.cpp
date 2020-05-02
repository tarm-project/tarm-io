/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "io/global/Configuration.h"
#include "io/UdpClient.h"

struct ConfigurationTest : public testing::Test,
                           public LogRedirector {
};

TEST_F(ConfigurationTest, buffer_size) {
    const auto min_receive_buf_size = io::global::min_receive_buffer_size();
    const auto default_receive_buf_size = io::global::default_receive_buffer_size();
    const auto max_receive_buf_size = io::global::max_receive_buffer_size();
    EXPECT_NE(0, min_receive_buf_size);
    EXPECT_NE(0, default_receive_buf_size);
    EXPECT_NE(0, max_receive_buf_size);
    EXPECT_LE(min_receive_buf_size, default_receive_buf_size);
    EXPECT_LE(default_receive_buf_size, max_receive_buf_size);

    const auto min_send_buf_size = io::global::min_send_buffer_size();
    const auto default_send_buf_size = io::global::default_send_buffer_size();
    const auto max_send_buf_size = io::global::max_send_buffer_size();
    EXPECT_NE(0, min_send_buf_size);
    EXPECT_NE(0, default_send_buf_size);
    EXPECT_NE(0, max_send_buf_size);
    EXPECT_LE(min_send_buf_size, default_send_buf_size);
    EXPECT_LE(default_send_buf_size, max_send_buf_size);

    std::cout << io::global::min_receive_buffer_size() << std::endl;
    std::cout << io::global::default_receive_buffer_size() << std::endl;
    std::cout << io::global::max_receive_buffer_size() << std::endl;

    std::cout << "===" << std::endl;

    std::cout << io::global::min_send_buffer_size() << std::endl;
    std::cout << io::global::default_send_buffer_size() << std::endl;
    std::cout << io::global::max_send_buffer_size() << std::endl;

    // TODO: move this test to Udp
    io::EventLoop loop;
    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({"127.0.0.1", 1500}));
    auto error = client->set_receive_buffer_size(io::global::max_receive_buffer_size());
    EXPECT_FALSE(error) << error.string();

    const auto buf_size = client->receive_buffer_size();
    EXPECT_FALSE(buf_size.error);
    EXPECT_EQ(io::global::max_receive_buffer_size(), buf_size.size);

    client->schedule_removal();

    loop.run();
}

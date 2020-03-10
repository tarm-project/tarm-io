#include "UTCommon.h"

#include "io/global/Configuration.h"
#include "io/UdpClient.h"

struct ConfigurationTest : public testing::Test,
                           public LogRedirector {
};

TEST_F(ConfigurationTest, receive_buffer_size) {
    std::cout << io::global::min_receive_buffer_size() << std::endl;
    std::cout << io::global::default_receive_buffer_size() << std::endl;
    std::cout << io::global::max_receive_buffer_size() << std::endl;

    std::cout << "===" << std::endl;

    std::cout << io::global::min_send_buffer_size() << std::endl;
    std::cout << io::global::default_send_buffer_size() << std::endl;
    std::cout << io::global::max_send_buffer_size() << std::endl;

    io::EventLoop loop;
    auto client = new io::UdpClient(loop, {"127.0.0.1", 1500});
    auto error = client->set_receive_buffer_size(io::global::max_receive_buffer_size());
    EXPECT_FALSE(error) << error.string();

    const auto buf_size = client->receive_buffer_size();
    EXPECT_FALSE(buf_size.error);
    EXPECT_EQ(io::global::max_receive_buffer_size(), buf_size.size);

    client->schedule_removal();

    loop.run();
}

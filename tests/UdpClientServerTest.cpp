
#include "UTCommon.h"

#include "io/UdpClient.h"
#include "io/UdpServer.h"
#include "io/ScopeExitGuard.h"
#include "io/Timer.h"

#include <cstdint>
/*#include <thread>
#include <vector>
#include <memory>
*/

struct UdpClientServerTest : public testing::Test,
                             public LogRedirector {

protected:
    std::uint16_t m_default_port = 31541;
    std::string m_default_addr = "127.0.0.1";
};

TEST_F(UdpClientServerTest, client_default_constructor) {
    io::EventLoop loop;
    auto client = new io::UdpClient(loop);

    client->schedule_removal();
    ASSERT_EQ(0, loop.run());
}

TEST_F(UdpClientServerTest, server_default_constructor) {
    io::EventLoop loop;
    auto server = new io::UdpServer(loop);

    server->schedule_removal();
    ASSERT_EQ(0, loop.run());
}

TEST_F(UdpClientServerTest, server_bind) {
    io::EventLoop loop;

    auto server = new io::UdpServer(loop);
    auto error = server->bind(m_default_addr, m_default_port);
    ASSERT_FALSE(error);
    server->schedule_removal();

    ASSERT_EQ(0, loop.run());
}

#if defined(__APPLE__) || defined(__linux__)
// Windows does not have privileged ports
TEST_F(UdpClientServerTest, bind_privileged) {
    io::EventLoop loop;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(io::Error(io::StatusCode::PERMISSION_DENIED), server->bind(m_default_addr, 80));
    server->schedule_removal();

    ASSERT_EQ(0, loop.run());
}
#endif

TEST_F(UdpClientServerTest, 1_client_sends_data_to_server) {
    io::EventLoop loop;

    const std::string message = "Hello world!";

    bool data_sent = false;
    bool data_received = false;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(io::Error(0), server->bind(m_default_addr, m_default_port));
    server->start_receive([&](io::UdpServer& server, std::uint32_t addr, std::uint16_t port, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        data_received = true;
        std::string s(data.buf.get(), data.size);
        EXPECT_EQ(message, s);
        server.schedule_removal();
    });

    auto client = new io::UdpClient(loop);
    client->send_data(message, 0x7F000001, m_default_port,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            data_sent = true;
            client.schedule_removal();
        });

    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(data_sent);
    EXPECT_TRUE(data_received);
}

TEST_F(UdpClientServerTest, send_larger_than_ethernet_mtu) {
    io::EventLoop loop;

    std::size_t SIZE = 5000;

    bool data_sent = false;
    bool data_received = false;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(io::Error(0), server->bind(m_default_addr, m_default_port));
    server->start_receive([&](io::UdpServer& server, std::uint32_t addr, std::uint16_t port, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        EXPECT_EQ(SIZE, data.size);
        data_received = true;

        for (size_t i = 0; i < SIZE / 2; ++i) {
            ASSERT_EQ(i, *(reinterpret_cast<const std::uint16_t*>(data.buf.get()) + i))  << "i =" << i;
        }

        server.schedule_removal();
    });

    std::shared_ptr<char> message(new char[SIZE], std::default_delete<char[]>());
    for (size_t i = 0; i < SIZE / 2; ++i) {
        *(reinterpret_cast<std::uint16_t*>(message.get()) + i) = i;
    }

    auto client = new io::UdpClient(loop);
    client->send_data(message, SIZE, 0x7F000001, m_default_port,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            data_sent = true;
            client.schedule_removal();
        });

    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(data_sent);
    EXPECT_TRUE(data_received);
}

TEST_F(UdpClientServerTest, send_larger_than_allowed_to_send) {
    io::EventLoop loop;

    std::size_t SIZE = 100 * 1024;

    std::shared_ptr<char> message(new char[SIZE], std::default_delete<char[]>());
    std::memset(message.get(), 0, SIZE);

    auto client = new io::UdpClient(loop);
    client->send_data(message, SIZE, 0x7F000001, m_default_port,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::MESSAGE_TOO_LONG, error.code());
            client.schedule_removal();
        });

    ASSERT_EQ(0, loop.run());
}

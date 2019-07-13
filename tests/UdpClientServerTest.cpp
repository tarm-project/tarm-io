
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
    ASSERT_TRUE(server->bind(m_default_addr, m_default_port).ok());
    server->schedule_removal();

    ASSERT_EQ(0, loop.run());
}

TEST_F(UdpClientServerTest, bind_privileged) {
    io::EventLoop loop;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(io::Status(io::StatusCode::PERMISSION_DENIED), server->bind(m_default_addr, 80));
    server->schedule_removal();

    ASSERT_EQ(0, loop.run());
}

TEST_F(UdpClientServerTest, 1_client_sends_data_to_server) {
    io::EventLoop loop;

    const std::string message = "Hello world!";

    bool data_sent = false;
    bool data_received = false;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(0, server->bind(m_default_addr, m_default_port));
    server->start_receive([&](io::UdpServer& server, std::uint32_t addr, std::uint16_t port, const char* buf, std::size_t size, const io::Status& status) {
        data_received = true;
        std::string s(buf, size);
        EXPECT_EQ(message, s);
        server.schedule_removal();
    });

    auto client = new io::UdpClient(loop);
    client->send_data(message, 0x7F000001, m_default_port,
        [&](io::UdpClient& client, const io::Status& status) {
            EXPECT_TRUE(status.ok());
            data_sent = true;
            client.schedule_removal();
        });

    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(data_sent);
    EXPECT_TRUE(data_received);
}

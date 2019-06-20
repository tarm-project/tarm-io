#include "UTCommon.h"

#include "io/TcpClient.h"
#include "io/TcpServer.h"

#include <cstdint>

struct TcpClientServerTest : public testing::Test,
                             public LogRedirector {

protected:
    std::uint16_t m_default_port = 31540;
    std::string m_default_addr = "127.0.0.1";
};

TEST_F(TcpClientServerTest, server_default_constructor) {
    io::EventLoop loop;
    io::TcpServer server(loop);

    ASSERT_EQ(0, loop.run());
}

TEST_F(TcpClientServerTest, client_default_constructor) {
    io::EventLoop loop;
    auto client = new io::TcpClient(loop);

    ASSERT_EQ(0, loop.run());
    client->schedule_removal();
}

TEST_F(TcpClientServerTest, 1_client_sends_data_to_server) {
    io::EventLoop loop;

    const std::string message = "Hello world!";

    bool data_received = false;

    io::TcpServer server(loop);
    ASSERT_EQ(0, server.bind("0.0.0.0", m_default_port));
    server.listen([&](io::TcpServer& server, io::TcpClient& client) -> bool {
        return true;
    },
    [&](io::TcpServer& server, io::TcpClient& client, const char* buf, size_t size) {
        data_received = true;
        EXPECT_EQ(size, message.length());
        EXPECT_EQ(buf, message);
        server.shutdown();
    });

    bool data_sent = false;

    auto client = new io::TcpClient(loop);
    client->connect(m_default_addr, m_default_port, [&](io::TcpClient& client) {
        client.send_data(message, [&](io::TcpClient& client) {
            data_sent = true;
            client.schedule_removal();
        });
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(data_sent);
    EXPECT_TRUE(data_received);
}



// TODO: client and server on separate threads
// TODO: server sends data to client frirs, right after connecting
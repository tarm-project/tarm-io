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
    io::TcpServer server(loop);

    ASSERT_EQ(0, server.bind("0.0.0.0", m_default_port));
    server.listen([&](io::TcpServer& server, io::TcpClient& client) -> bool {
        return true;
    },
    [&](io::TcpServer& server, io::TcpClient& client, const char* buf, size_t size) {
        std::cout.write(buf, size);
        server.shutdown();
    });

    auto client = new io::TcpClient(loop);
    client->connect(m_default_addr, m_default_port, [](io::TcpClient& client) {
        std::cout << "Connected!!!" << std::endl;
        client.send_data("Hello world!\n", [](io::TcpClient& client) {
            std::cout << "Data sent!!!" << std::endl;

            client.schedule_removal();
        });
    });

    ASSERT_EQ(0, loop.run());
}

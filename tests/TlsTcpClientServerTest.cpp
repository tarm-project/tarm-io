#include "UTCommon.h"

#include "io/TlsTcpClient.h"
#include "io/TlsTcpServer.h"

struct TlsTcpClientServerTest : public testing::Test,
                                public LogRedirector {

protected:
    std::uint16_t m_default_port = 32540;
    std::string m_default_addr = "127.0.0.1";
};
/*
TEST_F(TlsTcpClientServerTest,  constructor) {
    //this->log_to_stdout();

    io::EventLoop loop;
    auto client = new io::TlsTcpClient(loop);

    client->connect("64.233.162.113", 443,
        [](io::TlsTcpClient& client, const io::Error& error) {
            std::cout << "Connected!" << std::endl;
            client.send_data("GET / HTTP/1.0\r\n\r\n");
        },
        [](io::TlsTcpClient& client, const char* buf, size_t size) {
            std::cout.write(buf, size);
        },
        [](io::TlsTcpClient& client, const io::Error& error) {
            client.schedule_removal();
        });

    ASSERT_EQ(0, loop.run());
}
*/

TEST_F(TlsTcpClientServerTest, constructor) {
    io::EventLoop loop;

    io::TlsTcpServer server(loop);
    server.bind("0.0.0.0", 12345);
    server.listen([&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client) -> bool {
        return true;
    },
    [&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client, const char* buf, std::size_t size) {
        std::cout.write(buf, size);
    });

    ASSERT_EQ(0, loop.run());
}

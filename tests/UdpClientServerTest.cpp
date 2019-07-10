
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

TEST_F(UdpClientServerTest, server_default_constructor) {
    io::EventLoop loop;
    auto server = new io::UdpServer(loop);

    ASSERT_EQ(0, loop.run());
    server->schedule_removal();// TODO: describe in docs that schedule removal after loop  run is valid case
}

TEST_F(UdpClientServerTest, client_default_constructor) {
    io::EventLoop loop;
    auto client = new io::UdpClient(loop);

    ASSERT_EQ(0, loop.run());
    client->schedule_removal();
}

TEST_F(UdpClientServerTest, DISABLED_1_client_sends_data_to_server) {
    io::EventLoop loop;

    const std::string message = "Hello world!";

    bool data_received = false;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(0, server->bind(m_default_addr, m_default_port));
    server->start_receive([&](io::UdpServer&, std::uint32_t addr, std::uint16_t port, const char* buf, std::size_t size, const io::Status& status) {
        std::cout.write(buf, size);
        std::cout << std::endl;
    });

    auto client = new io::UdpClient(loop);
    std::shared_ptr<char> buf(new char);
    *buf = 'a';
    client->send_data(buf, 1, 0x7F000001, 31541);

    ASSERT_EQ(0, loop.run());
}

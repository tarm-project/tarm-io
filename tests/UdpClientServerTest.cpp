
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
    std::uint16_t m_default_port = 31540;
    std::string m_default_addr = "127.0.0.1";
};

TEST_F(UdpClientServerTest, server_default_constructor) {
    io::EventLoop loop;
    io::UdpServer server(loop);

    ASSERT_EQ(0, loop.run());
}


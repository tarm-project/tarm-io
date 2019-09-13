#include "UTCommon.h"

#include "io/TlsTcpClient.h"

struct TlsTcpClientServerTest : public testing::Test,
                                public LogRedirector {

protected:
    std::uint16_t m_default_port = 32540;
    std::string m_default_addr = "127.0.0.1";
};

TEST_F(TlsTcpClientServerTest,  constructor) {

}
#include "UTCommon.h"

#include "io/Path.h"
#include "io/DtlsClient.h"
#include "io/DtlsServer.h"

#include <boost/dll.hpp>

struct DtlsClientServerTest : public testing::Test,
                              public LogRedirector {
protected:
    std::uint16_t m_default_port = 30541;
    std::string m_default_addr = "127.0.0.1";

    const io::Path m_test_path = boost::dll::program_location().parent_path().string();

    const io::Path m_cert_path = m_test_path / "certificate.pem";
    const io::Path m_key_path = m_test_path / "key.pem";
};

TEST_F(DtlsClientServerTest, default_constructor) {
    io::EventLoop loop;
    this->log_to_stdout();

    auto client = new io::DtlsClient(loop);
    client->connect("127.0.0.1", 12345,
        [](io::DtlsClient& client, const io::Error&) {
            std::cout << "Connected!!!" << std::endl;
            client.send_data("bla_bla_bla");
        },
        [](io::DtlsClient& client, const char* buf, size_t size) {
            std::cout.write(buf, size);
        }
    );

    ASSERT_EQ(0, loop.run());
}

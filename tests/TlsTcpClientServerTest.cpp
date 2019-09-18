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
    this->log_to_stdout();

    io::EventLoop loop;

    const std::string cert_name = "certificate.pem";
    const std::string key_name = "key.pem";

    io::TlsTcpServer server(loop, cert_name, key_name);
    server.bind("0.0.0.0", 12345);
    auto listen_result = server.listen([&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client) -> bool {
        return true;
    },
    [&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client, const char* buf, std::size_t size) {
        std::cout.write(buf, size);

        std::string str(buf);
        if (str.find("GET / HTTP/1.1") == 0) {
            std::cout << "!!!" << std::endl;

            std::string answer = "HTTP/1.1 200 OK\r\n"
            "Date: Tue, 28 May 2019 13:13:01 GMT\r\n"
            "Server: My\r\n"
            "Content-Length: 41\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "\r\n"
            "<html><body>Hello world!!!" + std::to_string(::rand() % 10) + "</body></html>";


            client.send_data(answer);
        } else {
            client.close();
        }
    });

    ASSERT_EQ(0, listen_result);

    ASSERT_EQ(0, loop.run());
}

// TODO: private key with password
// TODO: multiple private keys and certificates in one file??? https://www.openssl.org/docs/man1.0.2/man3/SSL_CTX_use_PrivateKey_file.html
// TODO DER (binary file) support???

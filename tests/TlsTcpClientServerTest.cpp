#include "UTCommon.h"

#include "io/TlsTcpClient.h"
#include "io/TlsTcpServer.h"

#include <vector>

struct TlsTcpClientServerTest : public testing::Test,
                                public LogRedirector {

protected:
    std::uint16_t m_default_port = 32541;
    std::string m_default_addr = "127.0.0.1";

    const std::string m_cert_name = "certificate.pem";
    const std::string m_key_name = "key.pem";
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

/*
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
*/


TEST_F(TlsTcpClientServerTest, client_send_data_to_server) {
    const std::string message = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_name, m_key_name);
    server.bind(m_default_addr, m_default_port);
    auto listen_result = server.listen([&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client) -> bool {
        ++server_on_connect_callback_count;
        return true;
    },
    [&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client, const char* buf, std::size_t size) {
        ++server_on_receive_callback_count;

        EXPECT_EQ(message.size(), size);
        std::string received_message(buf, size);
        EXPECT_EQ(message, received_message);

        server.shutdown();
    });
    ASSERT_EQ(0, listen_result);

    auto client = new io::TlsTcpClient(loop);

    client->connect(m_default_addr, m_default_port,
        [&](io::TlsTcpClient& client, const io::Error& error) {
            ++client_on_connect_callback_count;
            client.send_data(message, [&](io::TlsTcpClient& client, const io::Error& error) {
                ++client_on_send_callback_count;
                client.schedule_removal();
            });
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_send_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(1, client_on_send_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(1, server_on_receive_callback_count);
}

TEST_F(TlsTcpClientServerTest, client_send_small_chunks_to_server) {
    this->log_to_stdout();

    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    const std::vector<std::string> messages = {
        "a",
        "ab",
        "abc",
        "abcd",
        "abcde",
        "abcdef",
        "abcdefg",
        "abcdefgh",
        "abcdefghi",
        "abcdefghij",
    };

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_name, m_key_name);
    server.bind(m_default_addr, m_default_port);
    auto listen_result = server.listen([&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client) -> bool {
        ++server_on_connect_callback_count;
        return true;
    },
    [&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client, const char* buf, std::size_t size) {
        EXPECT_EQ(messages[server_on_receive_callback_count].size(), size);
        std::string received_message(buf, size);
        EXPECT_EQ(messages[server_on_receive_callback_count], received_message);

        ++server_on_receive_callback_count;
        if (server_on_receive_callback_count == messages.size()) {
            server.shutdown();
        }
    });
    ASSERT_EQ(0, listen_result);

    auto client = new io::TlsTcpClient(loop);

    client->connect(m_default_addr, m_default_port,
        [&](io::TlsTcpClient& client, const io::Error& error) {
            ++client_on_connect_callback_count;

            for (const auto& m: messages) {
                client.send_data(m,
                    [&](io::TlsTcpClient& client, const io::Error& error) {
                        ++client_on_send_callback_count;

                        if (client_on_send_callback_count == messages.size()) {
                            client.schedule_removal();
                        }
                    }
                );
            }
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_send_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1,               client_on_connect_callback_count);
    EXPECT_EQ(messages.size(), client_on_send_callback_count);
    EXPECT_EQ(1,               server_on_connect_callback_count);
    EXPECT_EQ(messages.size(), server_on_receive_callback_count);
}

TEST_F(TlsTcpClientServerTest, server_send_data_to_client) {
    this->log_to_stdout();

    const std::string message = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_receive_callback_count = 0;
    std::size_t server_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_name, m_key_name);
    server.bind(m_default_addr, m_default_port);
    auto listen_result = server.listen([&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client) -> bool {
        ++server_on_connect_callback_count;
        client.send_data(message, [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
                ++server_on_send_callback_count;

                //loop.execute_on_loop_thread([&]() {
                    server.shutdown();
                //});
            });
        return true;
    },
    [&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client, const char* buf, std::size_t size) {
        ++server_on_receive_callback_count;
    });
    ASSERT_EQ(0, listen_result);

    auto client = new io::TlsTcpClient(loop);

    client->connect(m_default_addr, m_default_port,
        [&](io::TlsTcpClient& client, const io::Error& error) {
            ++client_on_connect_callback_count;
        },
        [&](io::TlsTcpClient& client, const char* buf, std::size_t size) {
            ++client_on_receive_callback_count;

            EXPECT_EQ(message.size(), size);
            std::string received_message(buf, size);
            EXPECT_EQ(message, received_message);

            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_receive_callback_count);
    EXPECT_EQ(0, server_on_send_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(1, client_on_receive_callback_count);
    EXPECT_EQ(1, server_on_send_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);
}

// TODO: the same test as server_send_data_to_client but multiple sends

// TODO: private key with password
// TODO: multiple private keys and certificates in one file??? https://www.openssl.org/docs/man1.0.2/man3/SSL_CTX_use_PrivateKey_file.html
// TODO DER (binary file) support???

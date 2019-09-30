#include "UTCommon.h"

#include "io/Path.h"
#include "io/TlsTcpClient.h"
#include "io/TlsTcpServer.h"


#include <boost/dll.hpp>

#include <vector>
// TODO: if win32
//#include <openssl/applink.c>

struct TlsTcpClientServerTest : public testing::Test,
                                public LogRedirector {

protected:
    std::uint16_t m_default_port = 32541;
    std::string m_default_addr = "127.0.0.1";

    const io::Path m_test_path = boost::dll::program_location().parent_path().string();

    const io::Path m_cert_path = m_test_path / "certificate.pem";
    const io::Path m_key_path = m_test_path / "key.pem";
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

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);
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

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);
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

TEST_F(TlsTcpClientServerTest, client_send_simultaneous_multiple_chunks_to_server) {
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

    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    std::size_t server_message_counter = 0;

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);
    server.bind(m_default_addr, m_default_port);
    auto listen_result = server.listen([&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client) -> bool {
        ++server_on_connect_callback_count;
        return true;
    },
    [&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client, const char* buf, std::size_t size) {
        const auto& expected_message = messages[server_message_counter++];

        EXPECT_EQ(expected_message.size(), size);
        std::string received_message(buf, size);
        EXPECT_EQ(expected_message, received_message);

        ++server_on_receive_callback_count;

        if (server_on_receive_callback_count == messages.size()) {
            server.shutdown();
        }
    });
    ASSERT_EQ(0, listen_result);

    auto client = new io::TlsTcpClient(loop);

    client->connect(m_default_addr, m_default_port,
        [&](io::TlsTcpClient& client, const io::Error& error) {
            for (std::size_t i = 0; i < messages.size(); ++i) {
                client.send_data(messages[i],
                    [&](io::TlsTcpClient& client, const io::Error& error) {
                        ++client_on_send_callback_count;

                        if (client_on_send_callback_count == messages.size()) {
                            client.schedule_removal();
                        }
                    }
                );
            }

            ++client_on_connect_callback_count;
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

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);
    server.bind(m_default_addr, m_default_port);
    auto listen_result = server.listen([&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client) -> bool {
        ++server_on_connect_callback_count;
        client.send_data(message, [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
                ++server_on_send_callback_count;
                server.shutdown();
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

TEST_F(TlsTcpClientServerTest, server_send_small_chunks_to_client) {
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

    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_receive_callback_count = 0;
    std::size_t server_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);
    server.bind(m_default_addr, m_default_port);
    auto listen_result = server.listen([&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client) -> bool {
        ++server_on_connect_callback_count;

        for (const auto& m: messages) {
            client.send_data(m,
                [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
                    ++server_on_send_callback_count;

                    if (server_on_send_callback_count == messages.size()) {
                        server.shutdown();
                    }
                }
            );
        }

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
            EXPECT_EQ(messages[client_on_receive_callback_count].size(), size);
            std::string received_message(buf, size);
            EXPECT_EQ(messages[client_on_receive_callback_count], received_message);

            ++client_on_receive_callback_count;

            if (client_on_receive_callback_count == messages.size()) {
                client.schedule_removal();
            }
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_receive_callback_count);
    EXPECT_EQ(0, server_on_send_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1,               client_on_connect_callback_count);
    EXPECT_EQ(messages.size(), client_on_receive_callback_count);
    EXPECT_EQ(messages.size(), server_on_send_callback_count);
    EXPECT_EQ(1,               server_on_connect_callback_count);
    EXPECT_EQ(0,               server_on_receive_callback_count);
}

TEST_F(TlsTcpClientServerTest, server_send_simultaneous_multiple_chunks_to_client) {
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

    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_receive_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;
    std::size_t server_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;


    std::size_t client_message_counter = 0;

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);
    server.bind(m_default_addr, m_default_port);
    auto listen_result = server.listen([&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client) -> bool {
        ++server_on_connect_callback_count;

        for (std::size_t i = 0; i < messages.size(); ++i) {
            client.send_data(messages[i],
                [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    ++server_on_send_callback_count;

                    if (server_on_send_callback_count == messages.size()) {
                        server.shutdown();
                    }
                }
            );
        }

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
            const auto& expected_message = messages[client_message_counter++];

            EXPECT_EQ(expected_message.size(), size);
            std::string received_message(buf, size);
            EXPECT_EQ(expected_message, received_message);

            ++client_on_receive_callback_count;

            if (client_on_receive_callback_count == messages.size()) {
                client.schedule_removal();
            }
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_receive_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);
    EXPECT_EQ(0, server_on_send_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1,               client_on_connect_callback_count);
    EXPECT_EQ(messages.size(), client_on_receive_callback_count);
    EXPECT_EQ(0,               server_on_receive_callback_count);
    EXPECT_EQ(messages.size(), server_on_send_callback_count);
    EXPECT_EQ(1,               server_on_connect_callback_count);
}

TEST_F(TlsTcpClientServerTest, client_and_server_send_each_other_1_mb_data) {
    this->log_to_stdout();

    const std::size_t DATA_SIZE = 1024 * 1024;
    static_assert(DATA_SIZE % 4 == 0, "Data size should be divisible by 4");

    std::shared_ptr<char> client_buf(new char[DATA_SIZE], std::default_delete<char[]>());
    std::shared_ptr<char> server_buf(new char[DATA_SIZE], std::default_delete<char[]>());

    std::size_t i = 0;
    for (; i < DATA_SIZE; i+= 4) {
        *reinterpret_cast<std::uint32_t*>(client_buf.get() + i) = i / 4;
        *reinterpret_cast<std::uint32_t*>(server_buf.get() + i) = DATA_SIZE / 4 - i / 4;
    }

    std::size_t client_on_connect_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;

    std::size_t client_on_data_send_callback_count = 0;
    std::size_t server_on_data_send_callback_count = 0;

    std::size_t client_data_receive_size = 0;
    std::size_t server_data_receive_size = 0;

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);
    EXPECT_FALSE(server.bind(m_default_addr, m_default_port));
    auto listen_error = server.listen(
        [&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client) -> bool {
            ++server_on_connect_callback_count;
            client.send_data(server_buf, DATA_SIZE,
                [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
                    ++server_on_data_send_callback_count;
                    EXPECT_FALSE(error);

                    if (server_data_receive_size == DATA_SIZE) {
                        server.shutdown();
                    }
                }
            );
            return true;
        },
        [&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client, const char* buf, std::size_t size) {
            auto previous_size = server_data_receive_size;
            server_data_receive_size += size;

            for (std::size_t i = previous_size; i < server_data_receive_size; ++i) {
                ASSERT_EQ(client_buf.get()[i], buf[i - previous_size]);
            }

            if (server_data_receive_size == DATA_SIZE && server_on_data_send_callback_count) {
                server.shutdown();
            }
        }
    );
    EXPECT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop);
    client->connect(m_default_addr, m_default_port,
        [&](io::TlsTcpClient& client, const io::Error& error) {
            ++client_on_connect_callback_count;
            client.send_data(client_buf, DATA_SIZE,
                [&](io::TlsTcpClient& client, const io::Error& error) {
                    ++client_on_data_send_callback_count;
                    EXPECT_FALSE(error);

                    if (client_data_receive_size == DATA_SIZE) {
                        client.schedule_removal();
                    }
                }
            );
        },
        [&](io::TlsTcpClient& client, const char* buf, std::size_t size) {
            auto previous_size = client_data_receive_size;

            client_data_receive_size += size;

            for (std::size_t i = previous_size; i < client_data_receive_size; ++i) {
                ASSERT_EQ(server_buf.get()[i], buf[i - previous_size]);
            }

            if (client_data_receive_size == DATA_SIZE && client_on_data_send_callback_count) {
                client.schedule_removal();
            }
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, client_data_receive_size);
    EXPECT_EQ(0, server_data_receive_size);
    EXPECT_EQ(0, client_on_data_send_callback_count);
    EXPECT_EQ(0, server_on_data_send_callback_count);

    EXPECT_FALSE(loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(1, client_on_data_send_callback_count);
    EXPECT_EQ(1, server_on_data_send_callback_count);
    EXPECT_EQ(DATA_SIZE, client_data_receive_size);
    EXPECT_EQ(DATA_SIZE, server_data_receive_size);
}

TEST_F(TlsTcpClientServerTest, server_close_client_conection_after_accepting_some_data) {
    this->log_to_stdout();

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);
    server.bind(m_default_addr, m_default_port);
    auto listen_result = server.listen(
        [&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client) -> bool {
            return true;
        },
        [&](io::TlsTcpServer& server, io::TlsTcpConnectedClient& client, const char* buf, std::size_t size) {
            std::cout.write(buf, size);

            std::string str(buf, size);
            if (str == "0") {
                std::cout << "OK" << std::endl;

                client.send_data("OK");
            } else {
                std::cout << "Closing!!!!" << std::endl;
                client.close();
            }
        }
    );
    ASSERT_EQ(0, listen_result);

    auto client = new io::TlsTcpClient(loop);
    unsigned counter = 0;

    client->connect(m_default_addr, m_default_port,
        [&](io::TlsTcpClient& client, const io::Error& error) {
            client.send_data(std::to_string(counter++));
        },
        [&](io::TlsTcpClient& client, const char* buf, std::size_t size) {
            std::cout.write(buf, size);
            client.send_data(std::to_string(counter++));
        },
        [&](io::TlsTcpClient& client, const io::Error& error) {
            std::cout << "Client closed!" << std::endl;
            client.schedule_removal();
            server.shutdown(); // Note: shutdowning server from client callback
        }
    );

    ASSERT_EQ(0, loop.run());
}

// TODO: the same test as server_send_data_to_client but multiple sends
// TODO: SSL_renegotiate test

// TODO: private key with password
// TODO: multiple private keys and certificates in one file??? https://www.openssl.org/docs/man1.0.2/man3/SSL_CTX_use_PrivateKey_file.html
// TODO DER (binary file) support???

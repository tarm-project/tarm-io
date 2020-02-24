#include "UTCommon.h"

#include "io/Path.h"
#include "io/TlsTcpClient.h"
#include "io/TlsTcpServer.h"
#include "io/global/Version.h"

#include <vector>

struct TlsTcpClientServerTest : public testing::Test,
                                public LogRedirector {

protected:
    std::uint16_t m_default_port = 32541;
    std::string m_default_addr = "127.0.0.1";

    const io::Path m_test_path = exe_path().string();

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
    auto listen_error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client) {
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data) {
            std::cout.write(buf, size);

            std::string str(buf);
            if (str.find("GET / HTTP/1.1") == 0) {
                std::cout << "!!!" << std::endl;

                std::string answer = "HTTP/1.1 200 OK\r\n"
                "Date: Tue, 28 May 2019 13:13:01 GMT\r\n"
                "Server: My\r\
            n"
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

    ASSERT_FALSE(listen_error);

    ASSERT_EQ(0, loop.run());
}
*/

TEST_F(TlsTcpClientServerTest, schedule_removal_not_connected_client) {
    io::EventLoop loop;
    auto client = new io::TlsTcpClient(loop);
    client->schedule_removal();

    ASSERT_EQ(0, loop.run());
}

TEST_F(TlsTcpClientServerTest, is_open_not_connected_client) {
    io::EventLoop loop;
    auto client = new io::TlsTcpClient(loop);
    EXPECT_FALSE(client->is_open());
    client->schedule_removal();

    ASSERT_EQ(0, loop.run());
}

TEST_F(TlsTcpClientServerTest, client_send_without_connect_with_callback) {
    io::EventLoop loop;

    auto client = new io::TlsTcpClient(loop);
    client->send_data("Hello",
        [](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::SOCKET_IS_NOT_CONNECTED, error.code());
            client.schedule_removal();
        }
    );

    ASSERT_EQ(0, loop.run());
}

TEST_F(TlsTcpClientServerTest, client_connect_to_invalid_address) {
    io::EventLoop loop;

    std::size_t client_on_connect_count = 0;

    auto client = new io::TlsTcpClient(loop);
    client->connect({"0.0.0", m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
            EXPECT_FALSE(client.is_open());

            client.schedule_removal();
            ++client_on_connect_count;
        }
    );

    // TODO: is it OK to call this callback with error before loop run???
    // Looks like no because we should guarantee that no callbacks are executed before loop run() call
    EXPECT_EQ(1, client_on_connect_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_connect_count);
}

TEST_F(TlsTcpClientServerTest, DISABLED_client_send_without_connect_no_callback) {
    io::EventLoop loop;

    auto client = new io::TlsTcpClient(loop);
    client->send_data("Hello"); // Just do nothing and hope for miracle
    client->schedule_removal();

    ASSERT_EQ(0, loop.run());
}

TEST_F(TlsTcpClientServerTest, client_send_data_to_server_no_close_callbacks) {
    const std::string message = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);
    auto listen_error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_connect_callback_count;
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_callback_count;

            EXPECT_EQ(message.size(), data.size);
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ(message, received_message);

            server.shutdown();
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_connect_callback_count;
            client.send_data(message, [&](io::TlsTcpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
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

TEST_F(TlsTcpClientServerTest, client_send_data_to_server_with_close_callbacks) {
    const std::string message = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_send_callback_count = 0;
    std::size_t client_on_close_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;
    std::size_t server_on_close_callback_count = 0;

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);
    auto listen_error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_connect_callback_count;
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_callback_count;

            EXPECT_EQ(message.size(), data.size);
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ(message, received_message);

            server.shutdown();
        },
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_close_callback_count;
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_connect_callback_count;
            client.send_data(message, [&](io::TlsTcpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                ++client_on_send_callback_count;
                client.close();
            });
        },
        nullptr,
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.schedule_removal();
            ++client_on_close_callback_count;
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_send_callback_count);
    EXPECT_EQ(0, client_on_close_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);
    EXPECT_EQ(0, server_on_close_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(1, client_on_send_callback_count);
    EXPECT_EQ(1, client_on_close_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(1, server_on_receive_callback_count);
    EXPECT_EQ(1, server_on_close_callback_count);
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
    auto listen_error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_connect_callback_count;
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(messages[server_on_receive_callback_count].size(), data.size);
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ(messages[server_on_receive_callback_count], received_message);

            ++server_on_receive_callback_count;
            if (server_on_receive_callback_count == messages.size()) {
                server.shutdown();

         }
    });
    ASSERT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_connect_callback_count;

            for (const auto& m: messages) {
                client.send_data(m,
                    [&](io::TlsTcpClient& client, const io::Error& error) {
                        EXPECT_FALSE(error);
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

    auto listen_error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_connect_callback_count;
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            const auto& expected_message = messages[server_message_counter++];

            EXPECT_EQ(expected_message.size(), data.size);
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ(expected_message, received_message);

            ++server_on_receive_callback_count
        ;

        if (server_on_receive_callback_count == messages.size()) {
            server.shutdown();
        }
    });
    ASSERT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            for (std::size_t i = 0; i < messages.size(); ++i) {
                client.send_data(messages[i],
                    [&](io::TlsTcpClient& client, const io::Error& error) {
                        EXPECT_FALSE(error);
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
    const std::string message = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_receive_callback_count = 0;
    std::size_t server_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);

    auto listen_error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_connect_callback_count;
            client.send_data(message, [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    ++server_on_send_callback_count;
                    server.shutdown();
                });
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_callback_count;
        });
    ASSERT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_connect_callback_count;
        },
        [&](io::TlsTcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_receive_callback_count;

            EXPECT_EQ(message.size(), data.size);
            std::string received_message(data.buf.get(), data.size);
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

    auto listen_error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_connect_callback_count;

            for (const auto& m: messages) {
                client.send_data(m,
                    [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
                        EXPECT_FALSE(error);
                        ++server_on_send_callback_count;

                        if (server_on_send_callback_count == messages.size()) {
                            server.shutdown();
                        }

                 }
            );
        }
    },
    [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        ++server_on_receive_callback_count;
    });
    ASSERT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_connect_callback_count;
        },
        [&](io::TlsTcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(messages[client_on_receive_callback_count].size(), data.size);
            std::string received_message(data.buf.get(), data.size);
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

    auto listen_error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
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
    },
    [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        ++server_on_receive_callback_count;
    });
    ASSERT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_connect_callback_count;
        },
        [&](io::TlsTcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            const auto& expected_message = messages[client_message_counter++];

            EXPECT_EQ(expected_message.size(), data.size);
            std::string received_message(data.buf.get(), data.size);
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
    auto listen_error = server.listen({m_default_addr, m_default_port},
            [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
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
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            auto previous_size = server_data_receive_size;
            server_data_receive_size += data.size;

            for (std::size_t i = previous_size; i < server_data_receive_size; ++i) {
                ASSERT_EQ(client_buf.get()[i], data.buf.get()[i - previous_size]);
            }

            if (server_data_receive_size == DATA_SIZE && server_on_data_send_callback_count) {
                server.shutdown();
            }
        }
    );
    EXPECT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

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
        [&](io::TlsTcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            auto previous_size = client_data_receive_size;

            client_data_receive_size += data.size;

            for (std::size_t i = previous_size; i < client_data_receive_size; ++i) {
                ASSERT_EQ(server_buf.get()[i], data.buf.get()[i - previous_size]);
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
    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);

    auto listen_error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            std::string str(data.buf.get(), data.size);
            if (str == "0") {
                client.send_data("OK");
            } else {
                client.close();
            }
        }
    );

    ASSERT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop);
    unsigned counter = 0;

    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data(std::to_string(counter++));
        },
        [&](io::TlsTcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data(std::to_string(counter++));
        },
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.schedule_removal();
            server.shutdown(); // Note: shutdowning server from client callback
        }
    );

    ASSERT_EQ(0, loop.run());
}

TEST_F(TlsTcpClientServerTest, not_existing_certificate) {
    io::EventLoop loop;

    io::Path not_existing_path;
#if defined(__APPLE__) || defined(__linux__)
    not_existing_path = "/no/existing/path.pem";
#else
    not_existing_path = "C:\\no\\existing\\path.pem";
#endif

    io::TlsTcpServer server(loop, not_existing_path, m_key_path);

    std::size_t server_new_connection_callback_count = 0;
    std::size_t server_data_receive_callback_count = 0;

    auto error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_new_connection_callback_count;
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_callback_count;
        }
    );

    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::TLS_CERTIFICATE_FILE_NOT_EXIST, error.code()) << error.string();

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);

    ASSERT_EQ(0, loop.run());
}

TEST_F(TlsTcpClientServerTest, not_existing_key) {
    io::EventLoop loop;

    io::Path not_existing_path;
#if defined(__APPLE__) || defined(__linux__)
    not_existing_path = "/no/existing/path.pem";
#else
    not_existing_path = "C:\\no\\existing\\path.pem";
#endif

    io::TlsTcpServer server(loop, m_cert_path, not_existing_path);

    std::size_t server_new_connection_callback_count = 0;
    std::size_t server_data_receive_callback_count = 0;

    auto error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_new_connection_callback_count;
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_callback_count;
        }
    );

    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::TLS_PRIVATE_KEY_FILE_NOT_EXIST, error.code()) << error.string();

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);

    ASSERT_EQ(0, loop.run());
}

TEST_F(TlsTcpClientServerTest, invalid_certificate) {
    io::EventLoop loop;

    io::Path certificate_path = m_test_path / "invalid_certificate.pem";;
    io::TlsTcpServer server(loop, certificate_path, m_key_path);

    std::size_t server_new_connection_callback_count = 0;
    std::size_t server_data_receive_callback_count = 0;

    auto error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_new_connection_callback_count;
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_callback_count;
        }
    );

    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::TLS_CERTIFICATE_INVALID, error.code()) << error.string();

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);

    ASSERT_EQ(0, loop.run());
}

TEST_F(TlsTcpClientServerTest, invalid_private_key) {
    io::EventLoop loop;

    io::Path key_path = m_test_path / "invalid_key.pem";;
    io::TlsTcpServer server(loop, m_cert_path, key_path);

    std::size_t server_new_connection_callback_count = 0;
    std::size_t server_data_receive_callback_count = 0;

    auto error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_new_connection_callback_count;
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_callback_count;
        }
    );

    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::TLS_PRIVATE_KEY_INVALID, error.code()) << error.string();

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);

    ASSERT_EQ(0, loop.run());
}

TEST_F(TlsTcpClientServerTest, not_matching_certificate_and_key) {
    io::EventLoop loop;

    const io::Path cert_path = m_test_path / "not_matching_certificate.pem";
    const io::Path key_path = m_test_path / "not_matching_key.pem";
    io::TlsTcpServer server(loop, cert_path, key_path);

    std::size_t server_new_connection_callback_count = 0;
    std::size_t server_data_receive_callback_count = 0;

    auto error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_new_connection_callback_count;
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_callback_count;
        }
    );

    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::TLS_PRIVATE_KEY_AND_CERTIFICATE_NOT_MATCH, error.code()) << error.string();

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);

    ASSERT_EQ(0, loop.run());
}

TEST_F(TlsTcpClientServerTest, callbacks_order) {
    // Test description: in this test we check that 'connect' callback is called before 'data receive'
    //                   callback for TLS connections
    io::EventLoop loop;

    const std::string client_message = "client message";
    const std::string server_message = "server message";

    std::size_t server_new_connection_callback_count = 0;
    std::size_t server_data_receive_callback_count = 0;

    std::size_t client_new_connection_callback_count = 0;
    std::size_t client_data_receive_callback_count = 0;

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);
    auto listen_error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            ++server_new_connection_callback_count;
            EXPECT_EQ(0, server_data_receive_callback_count);

            client.send_data(server_message,
                [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                }
            );
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_callback_count;
            server.shutdown();
        }
    );

    ASSERT_FALSE(listen_error) << listen_error.string();

    auto client = new io::TlsTcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            ++client_new_connection_callback_count;
            EXPECT_EQ(0, client_data_receive_callback_count);

            client.send_data(client_message,
                [&](io::TlsTcpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                }
            );
        },
        [&](io::TlsTcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            ++client_data_receive_callback_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);

    EXPECT_EQ(0, client_new_connection_callback_count);
    EXPECT_EQ(0, client_data_receive_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, server_new_connection_callback_count);
    EXPECT_EQ(1, server_data_receive_callback_count);

    EXPECT_EQ(1, client_new_connection_callback_count);
    EXPECT_EQ(1, client_data_receive_callback_count);
}

TEST_F(TlsTcpClientServerTest, tls_negotiated_version) {
    std::size_t client_on_connect_callback_count = 0;

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);
    auto listen_error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(io::global::max_supported_tls_version(), client.negotiated_tls_version());
            server.shutdown();
        },
        nullptr
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop);

    EXPECT_EQ(io::TlsVersion::UNKNOWN, client->negotiated_tls_version());

    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(io::global::max_supported_tls_version(), client.negotiated_tls_version());
            ++client_on_connect_callback_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
}

TEST_F(TlsTcpClientServerTest, server_with_restricted_tls_version) {
    const std::string message = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    auto server = new io::TlsTcpServer(loop, m_cert_path, m_key_path,
        io::TlsVersionRange{io::global::min_supported_tls_version(), io::global::min_supported_tls_version()});
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_connect_callback_count;
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_callback_count;

            EXPECT_EQ(message.size(), data.size);
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ(message, received_message);

            server->schedule_removal();
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_connect_callback_count;
            client.send_data(message, [&](io::TlsTcpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
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

TEST_F(TlsTcpClientServerTest, client_with_restricted_tls_version) {
    const std::string message = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_path, m_key_path);
    auto listen_error = server.listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(io::global::min_supported_tls_version(), client.negotiated_tls_version());
            ++server_on_connect_callback_count;
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_callback_count;

            EXPECT_EQ(message.size(), data.size);
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ(message, received_message);

            server.shutdown();
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop,
        io::TlsVersionRange{io::global::min_supported_tls_version(), io::global::min_supported_tls_version()});

    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(io::global::min_supported_tls_version(), client.negotiated_tls_version());
            ++client_on_connect_callback_count;
            client.send_data(message, [&](io::TlsTcpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
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

TEST_F(TlsTcpClientServerTest, client_and_server_tls_version_mismatch) {
    if (io::global::min_supported_tls_version() == io::global::max_supported_tls_version()) {
        return;
    }

    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_receive_callback_count = 0;
    std::size_t client_on_close_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    auto server = new io::TlsTcpServer(loop, m_cert_path, m_key_path,
        io::TlsVersionRange{io::global::max_supported_tls_version(), io::global::max_supported_tls_version()});
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::TlsTcpConnectedClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            EXPECT_EQ(io::TlsVersion::UNKNOWN, client.negotiated_tls_version());
            ++server_on_connect_callback_count;
            server->schedule_removal();
        },
        [&](io::TlsTcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_callback_count;
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop,
        io::TlsVersionRange{io::global::min_supported_tls_version(), io::global::min_supported_tls_version()});
    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            EXPECT_EQ(io::TlsVersion::UNKNOWN, client.negotiated_tls_version());
            ++client_on_connect_callback_count;
            client.schedule_removal();
        },
        [&](io::TlsTcpClient&, const io::DataChunk&, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_receive_callback_count;
        },
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_close_callback_count;
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_receive_callback_count);
    EXPECT_EQ(0, client_on_close_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_receive_callback_count);
    EXPECT_EQ(0, client_on_close_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);
}

TEST_F(TlsTcpClientServerTest, server_with_invalid_tls_version_range) {
    // Note: min is greater than max
    if (io::global::min_supported_tls_version() == io::global::max_supported_tls_version()) {
        return;
    }

    io::EventLoop loop;

    io::TlsTcpServer server(loop, m_cert_path, m_key_path,
        io::TlsVersionRange{io::global::max_supported_tls_version(), io::global::min_supported_tls_version()});
    auto listen_error = server.listen({m_default_addr, m_default_port},
        nullptr,
        nullptr
    );
    EXPECT_TRUE(listen_error);
    EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, listen_error.code());

    ASSERT_EQ(0, loop.run());

}

TEST_F(TlsTcpClientServerTest, client_with_invalid_tls_version_range) {
    // Note: min is greater than max
    if (io::global::min_supported_tls_version() == io::global::max_supported_tls_version()) {
        return;
    }

    io::EventLoop loop;

    auto server = new io::TlsTcpServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        nullptr,
        nullptr
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::TlsTcpClient(loop,
        io::TlsVersionRange{io::global::max_supported_tls_version(), io::global::min_supported_tls_version()});
    client->connect({m_default_addr, m_default_port},
        [&](io::TlsTcpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            server->schedule_removal();
            client.schedule_removal();
        },
        nullptr,
        nullptr
    );

    ASSERT_EQ(0, loop.run());
}


// TODO: connect as TCP and send invalid data on various stages
// TODO: listen on invalid address
// TODO: listen on privileged port

// TODO: SSL_renegotiate test

// TODO: private key with password
// TODO: multiple private keys and certificates in one file??? https://www.openssl.org/docs/man1.0.2/man3/SSL_CTX_use_PrivateKey_file.html
// TODO DER (binary file) support???

// TODO: simultaneous connect attempts (multiple connect calls)


// TODO: tests with close callbacks (react on both sef-close and remote close)

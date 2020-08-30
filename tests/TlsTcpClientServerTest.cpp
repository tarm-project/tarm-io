/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "net/ProtocolVersion.h"
#include "net/TlsTcpClient.h"
#include "net/TlsTcpServer.h"
#include "fs/Path.h"

#include <thread>
#include <vector>

struct TlsClientServerTest : public testing::Test,
                             public LogRedirector {

protected:
    std::uint16_t m_default_port = 32541;
    std::string m_default_addr = "127.0.0.1";

    const io::fs::Path m_test_path = exe_path().string();

    const io::fs::Path m_cert_path = m_test_path / "certificate.pem";
    const io::fs::Path m_key_path = m_test_path / "key.pem";
};
/*
TEST_F(TlsClientServerTest,  constructor) {
    //this->log_to_stdout();

    io::EventLoop loop;
    auto client = new io::net::TlsClient(loop);

    client->connect("64.233.162.113", 443,
        [](io::net::TlsClient& client, const io::Error& error) {
            std::cout << "Connected!" << std::endl;
            client.send_data("GET / HTTP/1.0\r\n\r\n");
        },
        [](io::net::TlsClient& client, const char* buf, size_t size) {
            std::cout.write(buf, size);
        },
        [](io::net::TlsClient& client, const io::Error& error) {
            client.schedule_removal();
        });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}
*/

/*
TEST_F(TlsClientServerTest, constructor) {
    this->log_to_stdout();

    io::EventLoop loop;

    const std::string cert_name = "certificate.pem";
    const std::string key_name = "key.pem";

    auto server = new io::net::TlsServer(loop, cert_name, key_name);
    server.bind("0.0.0.0", 12345);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client) {
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data) {
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

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}
*/

TEST_F(TlsClientServerTest, schedule_removal_not_connected_client) {
    io::EventLoop loop;
    auto client = new io::net::TlsClient(loop);
    client->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TlsClientServerTest, is_open_not_connected_client) {
    io::EventLoop loop;
    auto client = new io::net::TlsClient(loop);
    EXPECT_FALSE(client->is_open());
    client->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TlsClientServerTest, client_send_without_connect_with_callback) {
    io::EventLoop loop;

    auto client = new io::net::TlsClient(loop);
    client->send_data("Hello",
        [](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::NOT_CONNECTED, error.code());
            client.schedule_removal();
        }
    );

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TlsClientServerTest, client_connect_to_invalid_address) {
    io::EventLoop loop;

    std::size_t client_on_connect_count = 0;

    auto client = new io::net::TlsClient(loop);
    client->connect({"0.0.0", m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
            EXPECT_FALSE(client.is_open());

            client.schedule_removal();
            ++client_on_connect_count;
        }
    );

    EXPECT_EQ(0, client_on_connect_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_count);
}

TEST_F(TlsClientServerTest, server_listen_on_invalid_address) {
    io::EventLoop loop;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({"1.2:333333aa.adf", m_default_port},
        nullptr,
        nullptr,
        nullptr
    );
    EXPECT_TRUE(listen_error);
    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, listen_error.code());

    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}


TEST_F(TlsClientServerTest, bind_privileged) {
    // Windows does not have privileged ports
    TARM_IO_TEST_SKIP_ON_WINDOWS();

    io::EventLoop loop;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, 100},
        nullptr,
        nullptr,
        nullptr
    );
    EXPECT_TRUE(listen_error);
    EXPECT_EQ(io::StatusCode::PERMISSION_DENIED, listen_error.code());

    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TlsClientServerTest, server_address_in_use) {
    io::EventLoop loop;

    auto server_1 = new io::net::TlsServer(loop, m_cert_path, m_key_path);
    auto listen_error_1 = server_1->listen({m_default_addr, m_default_port}, nullptr, nullptr, nullptr);
    EXPECT_FALSE(listen_error_1);

    auto server_2 = new io::net::TlsServer(loop, m_cert_path, m_key_path);
    auto listen_error_2 = server_2->listen({m_default_addr, m_default_port}, nullptr, nullptr, nullptr);
    EXPECT_TRUE(listen_error_2);
    EXPECT_EQ(io::StatusCode::ADDRESS_ALREADY_IN_USE, listen_error_2.code());

    server_1->schedule_removal();
    server_2->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TlsClientServerTest, client_send_without_connect_no_callback) {
    io::EventLoop loop;

    auto client = new io::net::TlsClient(loop);
    client->send_data("Hello", // Just do nothing and hope for miracle
        [](io::net::TlsClient& client, const io::Error& error){
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::NOT_CONNECTED, error.code());
        }
    );
    client->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TlsClientServerTest, client_send_data_to_server_no_close_callbacks) {
    const char message[] = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_connect_callback_count;
            EXPECT_EQ(0, server_on_receive_callback_count);
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(1, server_on_connect_callback_count);
            ++server_on_receive_callback_count;

            EXPECT_EQ(sizeof(message) - 1, data.size);
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ(message, received_message);

            server->shutdown([](io::net::TlsServer& server, const io::Error& error) {
                server.schedule_removal();
            });
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::net::TlsClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_connect_callback_count;
            client.send_data(message, sizeof(message) - 1,
                [&](io::net::TlsClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    ++client_on_send_callback_count;
                    client.schedule_removal();
                }
            );
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_send_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(1, client_on_send_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(1, server_on_receive_callback_count);
}

TEST_F(TlsClientServerTest, client_send_data_to_server_with_close_callbacks) {
    const std::string message = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_send_callback_count = 0;
    std::size_t client_on_close_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;
    std::size_t server_on_close_callback_count = 0;

    io::EventLoop loop;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_connect_callback_count;
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_receive_callback_count;

            EXPECT_EQ(message.size(), data.size);
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ(message, received_message);

            server->shutdown([](io::net::TlsServer& server, const io::Error& error) {server.schedule_removal();});
        },
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_close_callback_count;
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::net::TlsClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_connect_callback_count;
            client.send_data(message, [&](io::net::TlsClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                ++client_on_send_callback_count;
                client.close();
            });
        },
        nullptr,
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_close_callback_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_send_callback_count);
    EXPECT_EQ(0, client_on_close_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);
    EXPECT_EQ(0, server_on_close_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(1, client_on_send_callback_count);
    EXPECT_EQ(1, client_on_close_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(1, server_on_receive_callback_count);
    EXPECT_EQ(1, server_on_close_callback_count);
}

TEST_F(TlsClientServerTest, client_send_simultaneous_multiple_chunks_to_server) {
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

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);

    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_connect_callback_count;
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            const auto& expected_message = messages[server_message_counter++];

            EXPECT_EQ(expected_message.size(), data.size);
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ(expected_message, received_message);

            ++server_on_receive_callback_count
        ;

        if (server_on_receive_callback_count == messages.size()) {
            server->shutdown([](io::net::TlsServer& server, const io::Error& error) {server.schedule_removal();});
        }
    });
    ASSERT_FALSE(listen_error);

    auto client = new io::net::TlsClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            for (std::size_t i = 0; i < messages.size(); ++i) {
                client.send_data(messages[i],
                    [&](io::net::TlsClient& client, const io::Error& error) {
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

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1,               client_on_connect_callback_count);
    EXPECT_EQ(messages.size(), client_on_send_callback_count);
    EXPECT_EQ(1,               server_on_connect_callback_count);
    EXPECT_EQ(messages.size(), server_on_receive_callback_count);
}

TEST_F(TlsClientServerTest, server_send_data_to_client) {
    const char message[] = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_receive_callback_count = 0;
    std::size_t server_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);

    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_connect_callback_count;
            client.send_data(message, sizeof(message) - 1, // -1 is for last 0
                [&](io::net::TlsConnectedClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    ++server_on_send_callback_count;
                    server->shutdown([](io::net::TlsServer& server, const io::Error& error) {server.schedule_removal();});
                }
            );
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_callback_count;
        });
    ASSERT_FALSE(listen_error);

    auto client = new io::net::TlsClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_connect_callback_count;
        },
        [&](io::net::TlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_receive_callback_count;

            EXPECT_EQ(sizeof(message) - 1, data.size);
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

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(1, client_on_receive_callback_count);
    EXPECT_EQ(1, server_on_send_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);
}

TEST_F(TlsClientServerTest, server_send_simultaneous_multiple_chunks_to_client) {
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

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);

    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_connect_callback_count;

            for (std::size_t i = 0; i < messages.size(); ++i) {
                client.send_data(messages[i],
                    [&](io::net::TlsConnectedClient& client, const io::Error& error) {
                        EXPECT_FALSE(error);
                        ++server_on_send_callback_count;

                        if (server_on_send_callback_count == messages.size()) {
                            server->shutdown([](io::net::TlsServer& server, const io::Error& error) {server.schedule_removal();});

                     }
                }
            );
        }
    },
    [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        ++server_on_receive_callback_count;
    });
    ASSERT_FALSE(listen_error);

    auto client = new io::net::TlsClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_connect_callback_count;
        },
        [&](io::net::TlsClient& client, const io::DataChunk& data, const io::Error& error) {
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

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1,               client_on_connect_callback_count);
    EXPECT_EQ(messages.size(), client_on_receive_callback_count);
    EXPECT_EQ(0,               server_on_receive_callback_count);
    EXPECT_EQ(messages.size(), server_on_send_callback_count);
    EXPECT_EQ(1,               server_on_connect_callback_count);
}

TEST_F(TlsClientServerTest, client_and_server_send_each_other_1_mb_data) {
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

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
            [&](io::net::TlsConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                ++server_on_connect_callback_count;
                EXPECT_EQ(0, server_data_receive_size);

                client.send_data(server_buf, DATA_SIZE,
                    [&](io::net::TlsConnectedClient& client, const io::Error& error) {
                        ++server_on_data_send_callback_count;
                        EXPECT_FALSE(error);

                        if (server_data_receive_size == DATA_SIZE) {
                            server->shutdown([](io::net::TlsServer& server, const io::Error& error) {server.schedule_removal();});

                    }
                }
            );
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            auto previous_size = server_data_receive_size;
            server_data_receive_size += data.size;

            for (std::size_t i = previous_size; i < server_data_receive_size; ++i) {
                ASSERT_EQ(client_buf.get()[i], data.buf.get()[i - previous_size]);
            }

            if (server_data_receive_size == DATA_SIZE && server_on_data_send_callback_count) {
                server->shutdown([](io::net::TlsServer& server, const io::Error& error) {server.schedule_removal();});
            }
        }
    );
    EXPECT_FALSE(listen_error);

    auto client = new io::net::TlsClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            ++client_on_connect_callback_count;
            client.send_data(client_buf, DATA_SIZE,
                [&](io::net::TlsClient& client, const io::Error& error) {
                    ++client_on_data_send_callback_count;
                    EXPECT_FALSE(error);

                    if (client_data_receive_size == DATA_SIZE) {
                        client.schedule_removal();
                    }
                }
            );
        },
        [&](io::net::TlsClient& client, const io::DataChunk& data, const io::Error& error) {
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

TEST_F(TlsClientServerTest, server_close_connection_cause_client_close) {
    // Test description: checking that connection close from server side calls close callbacks for both client and server
    io::EventLoop loop;

    std::size_t server_on_close_count = 0;
    std::size_t clietn_on_close_count = 0;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.close();
        },
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_close_count;
            server->schedule_removal();
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::net::TlsClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data("!");
        },
        nullptr,
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++clietn_on_close_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_on_close_count);
    EXPECT_EQ(0, clietn_on_close_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_close_count);
    EXPECT_EQ(1, clietn_on_close_count);
}

TEST_F(TlsClientServerTest, server_close_client_conection_after_accepting_some_data) {
    io::EventLoop loop;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);

    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
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

    auto client = new io::net::TlsClient(loop);
    unsigned counter = 0;

    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data(std::to_string(counter++));
        },
        [&](io::net::TlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data(std::to_string(counter++));
        },
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.schedule_removal();
            server->shutdown([](io::net::TlsServer& server, const io::Error& error) {server.schedule_removal();}); // Note: shutdowning server from client callback
        }
    );

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TlsClientServerTest, not_existing_certificate) {
    io::EventLoop loop;

    io::fs::Path not_existing_path;
#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    not_existing_path = "/not/existing/path.pem";
#else
    not_existing_path = "C:\\no\\existing\\path.pem";
#endif

    auto server = new io::net::TlsServer(loop, not_existing_path, m_key_path);

    std::size_t server_new_connection_callback_count = 0;
    std::size_t server_data_receive_callback_count = 0;

    auto error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_new_connection_callback_count;
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_callback_count;
        }
    );

    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::TLS_CERTIFICATE_FILE_NOT_EXIST, error.code()) << error.string();

    server->schedule_removal();

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);
}

TEST_F(TlsClientServerTest, not_existing_key) {
    io::EventLoop loop;

    io::fs::Path not_existing_path;
#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    not_existing_path = "/not/existing/path.pem";
#else
    not_existing_path = "C:\\no\\existing\\path.pem";
#endif

    auto server = new io::net::TlsServer(loop, m_cert_path, not_existing_path);

    std::size_t server_new_connection_callback_count = 0;
    std::size_t server_data_receive_callback_count = 0;

    auto error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_new_connection_callback_count;
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_callback_count;
        }
    );

    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::TLS_PRIVATE_KEY_FILE_NOT_EXIST, error.code()) << error.string();

    server->schedule_removal();

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TlsClientServerTest, invalid_certificate) {
    io::EventLoop loop;

    io::fs::Path certificate_path = m_test_path / "invalid_certificate.pem";;
    auto server = new io::net::TlsServer(loop, certificate_path, m_key_path);

    std::size_t server_new_connection_callback_count = 0;
    std::size_t server_data_receive_callback_count = 0;

    auto error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_new_connection_callback_count;
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_callback_count;
        }
    );

    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::TLS_CERTIFICATE_INVALID, error.code()) << error.string();
    server->schedule_removal();

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TlsClientServerTest, invalid_private_key) {
    io::EventLoop loop;

    io::fs::Path key_path = m_test_path / "invalid_key.pem";;
    auto server = new io::net::TlsServer(loop, m_cert_path, key_path);

    std::size_t server_new_connection_callback_count = 0;
    std::size_t server_data_receive_callback_count = 0;

    auto error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_new_connection_callback_count;
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_callback_count;
        }
    );

    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::TLS_PRIVATE_KEY_INVALID, error.code()) << error.string();

    server->schedule_removal();

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);
}

TEST_F(TlsClientServerTest, not_matching_certificate_and_key) {
    io::EventLoop loop;

    const io::fs::Path cert_path = m_test_path / "not_matching_certificate.pem";
    const io::fs::Path key_path = m_test_path / "not_matching_key.pem";
    auto server = new io::net::TlsServer(loop, cert_path, key_path);

    std::size_t server_new_connection_callback_count = 0;
    std::size_t server_data_receive_callback_count = 0;

    auto error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_new_connection_callback_count;
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_callback_count;
        }
    );

    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::TLS_PRIVATE_KEY_AND_CERTIFICATE_NOT_MATCH, error.code()) << error.string();

    server->schedule_removal();

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);
}

TEST_F(TlsClientServerTest, callbacks_order) {
    // Test description: in this test we check that 'connect' callback is called before 'data receive'
    //                   callback for TLS connections
    io::EventLoop loop;

    const std::string client_message = "client message";
    const std::string server_message = "server message";

    std::size_t server_new_connection_callback_count = 0;
    std::size_t server_data_receive_callback_count = 0;

    std::size_t client_new_connection_callback_count = 0;
    std::size_t client_data_receive_callback_count = 0;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            ++server_new_connection_callback_count;
            EXPECT_EQ(0, server_data_receive_callback_count);

            client.send_data(server_message,
                [&](io::net::TlsConnectedClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                }
            );
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_callback_count;
            server->shutdown([](io::net::TlsServer& server, const io::Error& error) {server.schedule_removal();});
        }
    );

    ASSERT_FALSE(listen_error) << listen_error.string();

    auto client = new io::net::TlsClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            ++client_new_connection_callback_count;
            EXPECT_EQ(0, client_data_receive_callback_count);

            client.send_data(client_message,
                [&](io::net::TlsClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                }
            );
        },
        [&](io::net::TlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            ++client_data_receive_callback_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_new_connection_callback_count);
    EXPECT_EQ(0, server_data_receive_callback_count);

    EXPECT_EQ(0, client_new_connection_callback_count);
    EXPECT_EQ(0, client_data_receive_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_new_connection_callback_count);
    EXPECT_EQ(1, server_data_receive_callback_count);

    EXPECT_EQ(1, client_new_connection_callback_count);
    EXPECT_EQ(1, client_data_receive_callback_count);
}

TEST_F(TlsClientServerTest, tls_negotiated_version) {
    std::size_t client_on_connect_callback_count = 0;

    io::EventLoop loop;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(io::net::max_supported_tls_version(), client.negotiated_tls_version());
            server->shutdown([](io::net::TlsServer& server, const io::Error& error) {server.schedule_removal();});
        },
        nullptr
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::net::TlsClient(loop);

    EXPECT_EQ(io::net::TlsVersion::UNKNOWN, client->negotiated_tls_version());

    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(io::net::max_supported_tls_version(), client.negotiated_tls_version());
            ++client_on_connect_callback_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
}

TEST_F(TlsClientServerTest, server_with_restricted_tls_version) {
    const std::string message = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path,
        io::net::TlsVersionRange{io::net::min_supported_tls_version(), io::net::min_supported_tls_version()});
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_connect_callback_count;
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_receive_callback_count;

            EXPECT_EQ(message.size(), data.size);
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ(message, received_message);

            server->schedule_removal();
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::net::TlsClient(loop);

    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_on_connect_callback_count;
            client.send_data(message, [&](io::net::TlsClient& client, const io::Error& error) {
                EXPECT_FALSE(error) << error.string();
                ++client_on_send_callback_count;
                client.schedule_removal();
            });
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_send_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(1, client_on_send_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(1, server_on_receive_callback_count);
}

TEST_F(TlsClientServerTest, client_with_restricted_tls_version) {
    const std::string message = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(io::net::min_supported_tls_version(), client.negotiated_tls_version());
            ++server_on_connect_callback_count;
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_callback_count;

            EXPECT_EQ(message.size(), data.size);
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ(message, received_message);

            server->shutdown([](io::net::TlsServer& server, const io::Error& error) {server.schedule_removal();});
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::net::TlsClient(loop,
        io::net::TlsVersionRange{io::net::min_supported_tls_version(), io::net::min_supported_tls_version()});

    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(io::net::min_supported_tls_version(), client.negotiated_tls_version());
            ++client_on_connect_callback_count;
            client.send_data(message, [&](io::net::TlsClient& client, const io::Error& error) {
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

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(1, client_on_send_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(1, server_on_receive_callback_count);
}

TEST_F(TlsClientServerTest, client_and_server_tls_version_mismatch) {
    if (io::net::min_supported_tls_version() == io::net::max_supported_tls_version()) {
        TARM_IO_TEST_SKIP();
    }

    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_receive_callback_count = 0;
    std::size_t client_on_close_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path,
        io::net::TlsVersionRange{io::net::max_supported_tls_version(), io::net::max_supported_tls_version()});
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::net::TlsConnectedClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            EXPECT_EQ(io::net::TlsVersion::UNKNOWN, client.negotiated_tls_version());
            ++server_on_connect_callback_count;
            server->schedule_removal();
        },
        [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_callback_count;
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::net::TlsClient(loop,
        io::net::TlsVersionRange{io::net::min_supported_tls_version(), io::net::min_supported_tls_version()});
    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            EXPECT_EQ(io::net::TlsVersion::UNKNOWN, client.negotiated_tls_version());
            ++client_on_connect_callback_count;
            client.schedule_removal();
        },
        [&](io::net::TlsClient&, const io::DataChunk&, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_receive_callback_count;
        },
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_close_callback_count;
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_receive_callback_count);
    EXPECT_EQ(0, client_on_close_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_receive_callback_count);
    EXPECT_EQ(0, client_on_close_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);
}

TEST_F(TlsClientServerTest, server_with_invalid_tls_version_range) {
    // Note: min is greater than max
    if (io::net::min_supported_tls_version() == io::net::max_supported_tls_version()) {
        TARM_IO_TEST_SKIP();
    }

    io::EventLoop loop;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path,
        io::net::TlsVersionRange{io::net::max_supported_tls_version(), io::net::min_supported_tls_version()});
    auto listen_error = server->listen({m_default_addr, m_default_port},
        nullptr,
        nullptr
    );
    EXPECT_TRUE(listen_error);
    EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, listen_error.code());

    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TlsClientServerTest, client_with_invalid_tls_version_range) {
    // Note: min is greater than max
    if (io::net::min_supported_tls_version() == io::net::max_supported_tls_version()) {
        TARM_IO_TEST_SKIP();
    }

    io::EventLoop loop;

    auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        nullptr,
        nullptr
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::net::TlsClient(loop,
        io::net::TlsVersionRange{io::net::max_supported_tls_version(), io::net::min_supported_tls_version()});
    client->connect({m_default_addr, m_default_port},
        [&](io::net::TlsClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            server->schedule_removal();
            client.schedule_removal();
        },
        nullptr,
        nullptr
    );

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TlsClientServerTest, server_works_with_multiple_clients) {
    const std::size_t CLIENTS_COUNT = 20;
    const std::size_t BUF_SIZE = 1 * 1024 * 1024;

    std::thread server_thread([&]() {
        io::EventLoop loop;

        std::size_t on_server_close_count = 0;

        auto server = new io::net::TlsServer(loop, m_cert_path, m_key_path);
        auto listen_error = server->listen({m_default_addr, m_default_port},
            [&](io::net::TlsConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error) << error;
            },
            [&](io::net::TlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
                EXPECT_FALSE(error) << error;
                client.send_data(data.buf, data.size);
            },
            [&](io::net::TlsConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error) << error;

                // TODO: connected_clients_count is unreliable here
                //if (client.server().connected_clients_count() == 1) { // this is the last one

                if (++on_server_close_count == CLIENTS_COUNT) {
                    server->schedule_removal();
                }
            }
        );
        ASSERT_FALSE(listen_error);

        ASSERT_EQ(io::StatusCode::OK, loop.run());
    });


    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::vector<std::thread> client_threads;
    for (std::size_t i = 0; i < CLIENTS_COUNT; ++i) {
        client_threads.emplace_back([&]() {
            std::shared_ptr<char> buffer(new char[BUF_SIZE], std::default_delete<char[]>());
            for (std::size_t i = 0; i < BUF_SIZE; ++i) {
                buffer.get()[i] = ::rand() % 255;
            }

            io::EventLoop loop;

            auto client = new io::net::TlsClient(loop);
            client->connect({m_default_addr, m_default_port},
                [&](io::net::TlsClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error;
                    client.send_data(buffer, BUF_SIZE);
                },
                [&](io::net::TlsClient& client, const io::DataChunk& data, const io::Error& error) {
                    EXPECT_FALSE(error) << error;
                    for (std::size_t i = 0; i < data.size; ++i) {
                        ASSERT_EQ(buffer.get()[i + data.offset], data.buf.get()[i]) << "i: " << i;
                    }

                    if (data.offset + data.size == BUF_SIZE) {
                        client.close();
                    }
                },
                [&](io::net::TlsClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error;
                    client.schedule_removal();
                }
            );

            ASSERT_EQ(io::StatusCode::OK, loop.run());
        });
    }

    server_thread.join();
    for (auto& t : client_threads) {
        t.join();
    }
}

// TODO: connect as TCP and send invalid data on various stages

// TODO: SSL_renegotiate test

// TODO: private key with password
// TODO: multiple private keys and certificates in one file??? https://www.openssl.org/docs/man1.0.2/man3/SSL_CTX_use_PrivateKey_file.html
// TODO: DER (binary file) support???

/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "net/GenericMessageOrientedClient.h"
#include "net/TcpClient.h"
#include "net/TcpServer.h"

struct GenericMessageOrientedClientServerTest : public testing::Test,
                                                public LogRedirector {
    GenericMessageOrientedClientServerTest() {
    }

protected:
    std::uint16_t m_default_port = 31540;
    std::string m_default_addr = "127.0.0.1";
};

using TcpMessageOrientedClient = io::net::GenericMessageOrientedClient<io::net::TcpClient>;
using TcpClientPtr = std::unique_ptr<io::net::TcpClient, io::Removable::DefaultDelete>;

using TcpMessageOrientedServer = io::net::GenericMessageOrientedServer<io::net::TcpServer>;
using TcpServerPtr = std::unique_ptr<io::net::TcpServer, io::Removable::DefaultDelete>;

using TcpMessageOrientedConnectedClient = io::net::GenericMessageOrientedConnectedClient<io::net::TcpConnectedClient>;

TEST_F(GenericMessageOrientedClientServerTest, client_default_state) {
    io::EventLoop loop;

    TcpClientPtr tcp_client(new io::net::TcpClient(loop), io::Removable::default_delete());
    TcpMessageOrientedClient message_client(std::move(tcp_client));

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(GenericMessageOrientedClientServerTest, server_default_state) {
    io::EventLoop loop;

    TcpServerPtr tcp_server(new io::net::TcpServer(loop), io::Removable::default_delete());
    TcpMessageOrientedServer message_server(std::move(tcp_server));

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(GenericMessageOrientedClientServerTest, client_send_1_byte) {
    // Note: using raw TCP server to check the data
    io::EventLoop loop;

    std::size_t server_receive_bytes_count = 0;

    auto server = new io::net::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
        [&](io::net::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
        },
        [&](io::net::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_receive_bytes_count;
            EXPECT_EQ(1, server_receive_bytes_count);
            client.close();
        },
        [&](io::net::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            server->schedule_removal();
        }
    );
    EXPECT_FALSE(listen_error) << listen_error;

    TcpClientPtr tcp_client(new io::net::TcpClient(loop), io::Removable::default_delete());
    TcpMessageOrientedClient message_client(std::move(tcp_client));
    message_client.connect({m_default_addr, m_default_port},
        [&](TcpMessageOrientedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data("!", 1);
        },
        [&](TcpMessageOrientedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
        },
        [&](TcpMessageOrientedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
        }
    );

    EXPECT_EQ(0, server_receive_bytes_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_receive_bytes_count);
}

TEST_F(GenericMessageOrientedClientServerTest, client_send_multiple_bytes) {
    // Note: using raw TCP server to check the data
    io::EventLoop loop;

    std::size_t server_receive_bytes_count = 0;

    // 310 chars
    const char SEND_STRING[] = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::size_t SEND_SIZE = sizeof(SEND_STRING) - 1; // -1 is for last \0 byte

    const char RECEIVE_STRING[] = "\x82""\x36""abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::size_t RECEIVE_SIZE = sizeof(RECEIVE_STRING) - 1; // -1 is for last \0 byte

    auto server = new io::net::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
        [&](io::net::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
        },
        [&](io::net::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            EXPECT_EQ(0, std::memcmp(&RECEIVE_STRING[server_receive_bytes_count],
                                     data.buf.get() + server_receive_bytes_count,
                                     data.size)) << "server_receive_bytes_count: " << server_receive_bytes_count;

            server_receive_bytes_count += data.size;
            if (server_receive_bytes_count == RECEIVE_SIZE) {
                client.close();
            }
        },
        [&](io::net::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            server->schedule_removal();
        }
    );
    EXPECT_FALSE(listen_error) << listen_error;

    TcpClientPtr tcp_client(new io::net::TcpClient(loop), io::Removable::default_delete());
    TcpMessageOrientedClient message_client(std::move(tcp_client));
    message_client.connect({m_default_addr, m_default_port},
        [&](TcpMessageOrientedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            client.send_data(SEND_STRING, SEND_SIZE);
        },
        [&](TcpMessageOrientedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
        },
        [&](TcpMessageOrientedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
        }
    );

    EXPECT_EQ(0, server_receive_bytes_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(RECEIVE_SIZE, server_receive_bytes_count);
}

TEST_F(GenericMessageOrientedClientServerTest, client_receive_1_byte) {
    // Note: using raw TCP server to generate the data
    io::EventLoop loop;

    std::size_t server_on_connect_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;
    std::size_t client_on_connect_count = 0;
    std::size_t client_on_receive_count = 0;
    std::size_t client_on_close_count = 0;

    auto server = new io::net::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
        [&](io::net::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_connect_count;
            char buf[] = {0x01, '!'};
            client.send_data(buf, 2);
        },
        [&](io::net::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_receive_count;
        },
        [&](io::net::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_close_count;
            server->schedule_removal();
        }
    );
    EXPECT_FALSE(listen_error) << listen_error;

    TcpClientPtr tcp_client(new io::net::TcpClient(loop), io::Removable::default_delete());
    TcpMessageOrientedClient message_client(std::move(tcp_client));
    message_client.connect({m_default_addr, m_default_port},
        [&](TcpMessageOrientedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_connect_count;
        },
        [&](TcpMessageOrientedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_receive_count;
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ("!", received_message);
            client.client().close();
        },
        [&](TcpMessageOrientedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_close_count;
        }
    );

    EXPECT_EQ(0, client_on_connect_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(0, client_on_close_count);
    EXPECT_EQ(0, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_count);
    EXPECT_EQ(1, client_on_receive_count);
    EXPECT_EQ(1, client_on_close_count);
    EXPECT_EQ(1, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(1, server_on_close_count);
}

TEST_F(GenericMessageOrientedClientServerTest, client_receive_multiple_bytes) {
    // Note: using raw TCP server to generate the data
    const unsigned char BUF[] = {
        0x01,       'a',
        0x05,       'b', 'b', 'b', 'b', 'b',
        0x81, 0x00, 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c',
        0x08,       'd', 'd', 'd', 'd', 'd', 'd', 'd', 'd'
    };

    std::size_t server_on_connect_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;
    std::size_t client_on_connect_count = 0;
    std::size_t client_on_receive_count = 0;
    std::size_t client_on_close_count = 0;

    io::EventLoop loop;

    auto server = new io::net::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
        [&](io::net::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_connect_count;
            client.send_data(reinterpret_cast<const char*>(BUF), sizeof(BUF));
        },
        [&](io::net::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_receive_count;
        },
        [&](io::net::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_close_count;
            server->schedule_removal();
        }
    );
    EXPECT_FALSE(listen_error) << listen_error;

    TcpClientPtr tcp_client(new io::net::TcpClient(loop), io::Removable::default_delete());
    TcpMessageOrientedClient message_client(std::move(tcp_client));
    message_client.connect({m_default_addr, m_default_port},
        [&](TcpMessageOrientedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_connect_count;
        },
        [&](TcpMessageOrientedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_receive_count;
            std::string received_message(data.buf.get(), data.size);

            switch(client_on_receive_count) {
                case 1:
                    EXPECT_EQ("a", received_message);
                    break;
                case 2:
                    EXPECT_EQ("bbbbb", received_message);
                    break;
                case 3: {
                        const std::string EXPECTED(128, 'c');
                        EXPECT_EQ(EXPECTED, received_message);
                    }
                    break;
                case 4:
                    EXPECT_EQ("dddddddd", received_message);
                    client.client().close();
                    break;
                default:
                    FAIL() << "more message tha expected";
            }
        },
        [&](TcpMessageOrientedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_close_count;
        }
    );

    EXPECT_EQ(0, client_on_connect_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(0, client_on_close_count);
    EXPECT_EQ(0, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_count);
    EXPECT_EQ(4, client_on_receive_count);
    EXPECT_EQ(1, client_on_close_count);
    EXPECT_EQ(1, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(1, server_on_close_count);
}

TEST_F(GenericMessageOrientedClientServerTest, client_receive_large_messages) {
    // Note: using raw TCP server to generate the data
    std::vector<char> buf1(65535, 'a');
    std::vector<char> buf2(128000, 'b');

    ::tarm::io::detail::VariableLengthSize size1(buf1.size());
    ASSERT_TRUE(size1.is_complete());

    ::tarm::io::detail::VariableLengthSize size2(buf2.size());
    ASSERT_TRUE(size2.is_complete());

    for (std::size_t i = 0; i < buf1.size(); ++i) {
        buf1[i] = ::rand() % 255;
    }

    for (std::size_t i = 0; i < buf2.size(); ++i) {
        buf2[i] = ::rand() % 255;
    }

    std::size_t server_on_connect_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;
    std::size_t client_on_connect_count = 0;
    std::size_t client_on_receive_count = 0;
    std::size_t client_on_close_count = 0;

    io::EventLoop loop;

    auto server = new io::net::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
        [&](io::net::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_connect_count;

            client.send_data(reinterpret_cast<const char*>(size1.bytes()), size1.bytes_count());
            client.send_data(&buf1.front(), buf1.size());
            client.send_data(reinterpret_cast<const char*>(size2.bytes()), size2.bytes_count());
            client.send_data(&buf2.front(), buf2.size());
        },
        [&](io::net::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_receive_count;
        },
        [&](io::net::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_close_count;
            server->schedule_removal();
        }
    );
    EXPECT_FALSE(listen_error) << listen_error;

    TcpClientPtr tcp_client(new io::net::TcpClient(loop), io::Removable::default_delete());
    TcpMessageOrientedClient message_client(std::move(tcp_client));
    message_client.connect({m_default_addr, m_default_port},
        [&](TcpMessageOrientedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_connect_count;
        },
        [&](TcpMessageOrientedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_receive_count;
            if (client_on_receive_count == 1) {
                EXPECT_EQ(0, std::memcmp(&buf1.front(), data.buf.get(), data.size));
            } else {
                EXPECT_EQ(0, std::memcmp(&buf2.front(), data.buf.get(), data.size));
                client.client().close();
            }
        },
        [&](TcpMessageOrientedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_close_count;
        }
    );

    EXPECT_EQ(0, client_on_connect_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(0, client_on_close_count);
    EXPECT_EQ(0, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_count);
    EXPECT_EQ(2, client_on_receive_count);
    EXPECT_EQ(1, client_on_close_count);
    EXPECT_EQ(1, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(1, server_on_close_count);
}

TEST_F(GenericMessageOrientedClientServerTest, client_receive_too_large_messages) {

    const std::vector<char> large_buf(TcpMessageOrientedClient::DEFAULT_MAX_SIZE + 1, 'a');
    const std::string small_message("I'm small!!!!");

    ::tarm::io::detail::VariableLengthSize size1(large_buf.size());
    ASSERT_TRUE(size1.is_complete());
    ::tarm::io::detail::VariableLengthSize size2(small_message.size());
    ASSERT_TRUE(size2.is_complete());

    std::size_t server_on_connect_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;
    std::size_t client_on_connect_count = 0;
    std::size_t client_on_receive_count = 0;
    std::size_t client_on_close_count = 0;

    io::EventLoop loop;

    auto server = new io::net::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
        [&](io::net::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_connect_count;

            client.send_data(reinterpret_cast<const char*>(size1.bytes()), size1.bytes_count());
            client.send_data(&large_buf.front(), large_buf.size());
            // next message with lower size is OK
            client.send_data(reinterpret_cast<const char*>(size2.bytes()), size2.bytes_count());
            client.send_data(&small_message.front(), small_message.size());
        },
        [&](io::net::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_receive_count;
        },
        [&](io::net::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_close_count;
            server->schedule_removal();
        }
    );
    EXPECT_FALSE(listen_error) << listen_error;

    TcpClientPtr tcp_client(new io::net::TcpClient(loop), io::Removable::default_delete());
    TcpMessageOrientedClient message_client(std::move(tcp_client));
    message_client.connect({m_default_addr, m_default_port},
        [&](TcpMessageOrientedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_connect_count;
        },
        [&](TcpMessageOrientedClient& client, const io::DataChunk& data, const io::Error& error) {
            ++client_on_receive_count;
            if (client_on_receive_count == 1) {
                EXPECT_TRUE(error);
                EXPECT_EQ(io::StatusCode::MESSAGE_TOO_LONG, error.code());
            } else {
                EXPECT_FALSE(error) << error;
                EXPECT_EQ(0, std::memcmp(small_message.c_str(), data.buf.get(), small_message.size()));
                client.client().close();
            }
        },
        [&](TcpMessageOrientedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_close_count;
        }
    );

    EXPECT_EQ(0, client_on_connect_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(0, client_on_close_count);
    EXPECT_EQ(0, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_count);
    EXPECT_EQ(2, client_on_receive_count);
    EXPECT_EQ(1, client_on_close_count);
    EXPECT_EQ(1, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(1, server_on_close_count);
}

TEST_F(GenericMessageOrientedClientServerTest, server_receive_1_message) {
    // Note: using raw TCP client to generate the data

    std::size_t server_on_connect_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;
    std::size_t client_on_connect_count = 0;
    std::size_t client_on_receive_count = 0;
    std::size_t client_on_close_count = 0;

    io::EventLoop loop;

    auto tcp_client = new io::net::TcpClient(loop);
    tcp_client->connect({m_default_addr, m_default_port},
        [&](io::net::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data("\x01", 1);
            client.send_data("!", 1);
            ++client_on_connect_count;
        },
        [&](io::net::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_receive_count;
        },
        [&](io::net::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client_on_close_count++;
            client.schedule_removal();
        }
    );

    TcpServerPtr tcp_server(new io::net::TcpServer(loop), io::Removable::default_delete());
    TcpMessageOrientedServer message_server(std::move(tcp_server));
    auto listen_error = message_server.listen({"0.0.0.0", m_default_port},
        [&](TcpMessageOrientedConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_connect_count;
        },
        [&](TcpMessageOrientedConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_receive_count;
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ("!", received_message);
            message_server.server().close();
        },
        [&](TcpMessageOrientedConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_on_close_count;
        }
    );
    EXPECT_FALSE(listen_error) << listen_error;

    EXPECT_EQ(0, client_on_connect_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(0, client_on_close_count);
    EXPECT_EQ(0, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(1, client_on_close_count);
    EXPECT_EQ(1, server_on_connect_count);
    EXPECT_EQ(1, server_on_receive_count);
    EXPECT_EQ(1, server_on_close_count);
}

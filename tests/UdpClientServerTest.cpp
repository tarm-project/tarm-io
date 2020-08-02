/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "net/UdpClient.h"
#include "net/UdpServer.h"
#include "ScopeExitGuard.h"
#include "Timer.h"

#include <cstdint>
#include <numeric>
#include <unordered_map>
#include <thread>

struct UdpClientServerTest : public testing::Test,
                             public LogRedirector {

protected:
    std::uint16_t m_default_port = 31541;
    std::string m_default_addr = "127.0.0.1";
};

TEST_F(UdpClientServerTest, client_default_state) {
    io::EventLoop loop;
    auto client = new io::net::UdpClient(loop);
    EXPECT_EQ(io::net::Endpoint::UNDEFINED, client->endpoint().type());

    client->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, server_default_state) {
    io::EventLoop loop;
    auto server = new io::net::UdpServer(loop);

    server->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, server_bind) {
    io::EventLoop loop;

    auto server = new io::net::UdpServer(loop);
    auto error = server->start_receive({m_default_addr, m_default_port}, nullptr);
    ASSERT_FALSE(error);
    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

// Windows does not have privileged ports
TEST_F(UdpClientServerTest, bind_privileged) {
#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    io::EventLoop loop;

    auto server = new io::net::UdpServer(loop);
    ASSERT_EQ(io::Error(io::StatusCode::PERMISSION_DENIED), server->start_receive({m_default_addr, 80}, nullptr));
    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
#else
    IO_TEST_SKIP();
#endif
}

TEST_F(UdpClientServerTest, server_address_in_use) {
    io::EventLoop loop;

    auto server_1 = new io::net::UdpServer(loop);
    auto listen_error_1 = server_1->start_receive({m_default_addr, m_default_port}, nullptr);
    EXPECT_FALSE(listen_error_1);

    auto server_2 = new io::net::UdpServer(loop);
    auto listen_error_2 = server_2->start_receive({m_default_addr, m_default_port}, nullptr);
    EXPECT_TRUE(listen_error_2);
    EXPECT_EQ(io::StatusCode::ADDRESS_ALREADY_IN_USE, listen_error_2.code());

    server_1->schedule_removal();
    server_2->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, server_invalid_address) {
    io::EventLoop loop;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({"100500", m_default_port}, nullptr);
    EXPECT_TRUE(listen_error);
    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, listen_error.code());

    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, client_invalid_address) {
    io::EventLoop loop;

    auto client = new io::net::UdpClient(loop);
    client->set_destination({"bla", m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
            client.schedule_removal();
        },
        nullptr
    );

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, client_remove_after_set_destination) {
    io::EventLoop loop;

    auto client = new io::net::UdpClient(loop);
    client->set_destination({"bla", m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
        },
        nullptr
    );

    client->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, 1_client_send_data_to_server) {
    io::EventLoop loop;

    const std::string message = "Hello world!";

    std::size_t server_on_data_receive_count = 0;
    std::size_t client_on_data_send_count = 0;
    std::size_t client_on_destination_set_count = 0;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            EXPECT_EQ(server, &peer.server());
            EXPECT_EQ("127.0.0.1", peer.endpoint().address_string());
            EXPECT_NE(0, peer.endpoint().port());

            ++server_on_data_receive_count;
            std::string s(data.buf.get(), data.size);
            EXPECT_EQ(message, s);
            server->schedule_removal();
        }
    );

    EXPECT_FALSE(listen_error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination(
        {0x7F000001u, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            EXPECT_EQ("127.0.0.1", client.endpoint().address_string());
            EXPECT_EQ(m_default_port, client.endpoint().port());

            ++client_on_destination_set_count;
            client.send_data(message,
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error;
                    ++client_on_data_send_count;
                    client.schedule_removal();
                }
            );
        }
    );

    EXPECT_EQ("", client->endpoint().address_string());
    EXPECT_EQ(0, client->endpoint().port());

    EXPECT_EQ(0, server_on_data_receive_count);
    EXPECT_EQ(0, client_on_data_send_count);
    EXPECT_EQ(0, client_on_destination_set_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_data_receive_count);
    EXPECT_EQ(1, client_on_data_send_count);
    EXPECT_EQ(1, client_on_destination_set_count);
}

TEST_F(UdpClientServerTest, client_get_buffer_size) {
    io::EventLoop loop;
    auto client = new io::net::UdpClient(loop);

    // At this point client has no receive callback nor destination set.
    // Even IP protocol version is undefined yet.
    const auto receive_buffer_1 = client->receive_buffer_size();
    EXPECT_TRUE(receive_buffer_1.error);

    const auto send_buffer_1 = client->send_buffer_size();
    EXPECT_TRUE(send_buffer_1.error);

    client->set_destination({0x7F000001u, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            const auto receive_buffer_2 = client.receive_buffer_size();
            EXPECT_FALSE(receive_buffer_2.error);
            EXPECT_NE(0, receive_buffer_2.size);

            const auto send_buffer_2 = client.send_buffer_size();
            EXPECT_FALSE(send_buffer_2.error);
            EXPECT_NE(0, send_buffer_2.size);

            client.schedule_removal();
        }
    );

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, server_get_buffer_size_1) {
    io::EventLoop loop;

    auto server = new io::net::UdpServer(loop);
    const auto receive_buffer = server->receive_buffer_size();
    EXPECT_TRUE(receive_buffer.error);

    const auto send_buffer = server->send_buffer_size();
    EXPECT_TRUE(send_buffer.error);

    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, server_get_buffer_size_2) {
    io::EventLoop loop;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer&, const io::DataChunk&, const io::Error&) {
        }
    );
    EXPECT_FALSE(listen_error);

    const auto receive_buffer = server->receive_buffer_size();
    EXPECT_FALSE(receive_buffer.error);
    EXPECT_NE(0, receive_buffer.size);

    const auto send_buffer = server->send_buffer_size();
    EXPECT_FALSE(send_buffer.error);
    EXPECT_NE(0, send_buffer.size);

    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, client_set_buffer_size) {
    io::EventLoop loop;
    auto client = new io::net::UdpClient(loop);

    EXPECT_EQ(io::Error(io::StatusCode::ADDRESS_NOT_AVAILABLE), client->set_send_buffer_size(4096));
    EXPECT_EQ(io::Error(io::StatusCode::ADDRESS_NOT_AVAILABLE), client->set_receive_buffer_size(4096));

    client->set_destination({0x7F000001u, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_EQ(io::Error(io::StatusCode::OK), client.set_send_buffer_size(4096));
            EXPECT_EQ(io::Error(io::StatusCode::OK), client.set_receive_buffer_size(4096));

            auto receive_buffer = client.receive_buffer_size();
            EXPECT_FALSE(receive_buffer.error);
            EXPECT_EQ(4096, receive_buffer.size);

            auto send_buffer = client.send_buffer_size();
            EXPECT_FALSE(send_buffer.error);
            EXPECT_EQ(4096, send_buffer.size);

            EXPECT_EQ(io::Error(io::StatusCode::INVALID_ARGUMENT), client.set_send_buffer_size(0));
            EXPECT_EQ(io::Error(io::StatusCode::INVALID_ARGUMENT), client.set_receive_buffer_size(0));

            receive_buffer = client.receive_buffer_size();
            EXPECT_FALSE(receive_buffer.error);
            EXPECT_EQ(4096, receive_buffer.size);

            send_buffer = client.send_buffer_size();
            EXPECT_FALSE(send_buffer.error);
            EXPECT_EQ(4096, send_buffer.size);

            client.schedule_removal();
        }
    );

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, server_set_buffer_size) {
    io::EventLoop loop;

    auto server = new io::net::UdpServer(loop);

    EXPECT_EQ(io::Error(io::StatusCode::ADDRESS_NOT_AVAILABLE), server->set_send_buffer_size(4096));
    EXPECT_EQ(io::Error(io::StatusCode::ADDRESS_NOT_AVAILABLE), server->set_receive_buffer_size(4096));

    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer&, const io::DataChunk&, const io::Error&) {
        }
    );
    EXPECT_FALSE(listen_error) << listen_error;

    EXPECT_EQ(io::Error(io::StatusCode::OK), server->set_send_buffer_size(4096));
    EXPECT_EQ(io::Error(io::StatusCode::OK), server->set_receive_buffer_size(4096));

    auto receive_buffer = server->receive_buffer_size();
    EXPECT_FALSE(receive_buffer.error);
    EXPECT_EQ(4096, receive_buffer.size);

    auto send_buffer = server->send_buffer_size();
    EXPECT_FALSE(send_buffer.error);
    EXPECT_EQ(4096, send_buffer.size);

    EXPECT_EQ(io::Error(io::StatusCode::INVALID_ARGUMENT), server->set_send_buffer_size(4000000000u));
    EXPECT_EQ(io::Error(io::StatusCode::INVALID_ARGUMENT), server->set_receive_buffer_size(4000000000u));

    receive_buffer = server->receive_buffer_size();
    EXPECT_FALSE(receive_buffer.error);
    EXPECT_EQ(4096, receive_buffer.size);

    send_buffer = server->send_buffer_size();
    EXPECT_FALSE(send_buffer.error);
    EXPECT_EQ(4096, send_buffer.size);

    EXPECT_EQ(io::Error(io::StatusCode::INVALID_ARGUMENT), server->set_send_buffer_size(0));
    EXPECT_EQ(io::Error(io::StatusCode::INVALID_ARGUMENT), server->set_receive_buffer_size(0));

    receive_buffer = server->receive_buffer_size();
    EXPECT_FALSE(receive_buffer.error);
    EXPECT_EQ(4096, receive_buffer.size);

    send_buffer = server->send_buffer_size();
    EXPECT_FALSE(send_buffer.error);
    EXPECT_EQ(4096, send_buffer.size);

    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, set_minimal_buffer_size) {
    io::EventLoop loop;

    const auto min_send_buffer_size = io::global::min_send_buffer_size();
    const auto min_receive_buffer_size = io::global::min_receive_buffer_size()
#ifdef TARM_IO_PLATFORM_MACOSX
    // At least on MAC OS X receive buffer should be larger (N + 16) than send buffer
    // to be able to receive packets of size N
    + 16
#endif
    ;

    std::size_t server_on_send_counter = 0;
    std::size_t server_on_receive_counter = 0;
    std::size_t client_on_send_counter = 0;
    std::size_t client_on_receive_counter = 0;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_counter;

            peer.send_data("!",
                [&](io::net::UdpPeer& peer, const io::Error& error) {
                    EXPECT_FALSE(error);
                    ++server_on_send_counter;
                    server->schedule_removal();
                }
            );
        }
    );
    EXPECT_FALSE(listen_error);
    EXPECT_FALSE(server->set_send_buffer_size(min_send_buffer_size));
    EXPECT_FALSE(server->set_receive_buffer_size(min_receive_buffer_size + 16));

    auto client = new io::net::UdpClient(loop);
    client->set_destination(
        {0x7F000001u, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            EXPECT_FALSE(client.set_send_buffer_size(min_send_buffer_size));
            EXPECT_FALSE(client.set_receive_buffer_size(min_receive_buffer_size + 16));

            std::shared_ptr<char> buf(new char[min_send_buffer_size], std::default_delete<char[]>());
            std::memset(buf.get(), 0, min_send_buffer_size);
            client.send_data(buf, min_send_buffer_size,
                [&](io::net::UdpClient& peer, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string();
                    ++client_on_send_counter;
                }
            );
        },
        [&](io::net::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++client_on_receive_counter;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_on_send_counter);
    EXPECT_EQ(0, server_on_receive_counter);
    EXPECT_EQ(0, client_on_send_counter);
    EXPECT_EQ(0, client_on_receive_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_send_counter);
    EXPECT_EQ(1, server_on_receive_counter);
    EXPECT_EQ(1, client_on_send_counter);
    EXPECT_EQ(1, client_on_receive_counter);
}

TEST_F(UdpClientServerTest, peer_identity_without_preservation_on_server) {
    io::EventLoop loop;

    const std::string client_message_1 = "message_1";
    const std::string client_message_2 = "close";

    std::size_t server_receive_counter = 0;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);

        if (server_receive_counter == 0) {
            peer.set_user_data(&server_receive_counter);
        } else {
            // This is new instance of io::net::UdpPeer
            EXPECT_EQ(nullptr, peer.user_data());
        }

        ++server_receive_counter;

        std::string s(data.buf.get(), data.size);
        if (s == client_message_2) {
            server->schedule_removal();
        }
    }); // note: callback without timeout set

    EXPECT_FALSE(listen_error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({0x7F000001u, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            client.send_data(client_message_1,
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    client.send_data(client_message_2,
                        [&](io::net::UdpClient& client, const io::Error& error) {
                            EXPECT_FALSE(error);
                            client.schedule_removal();
                        }
                    );
                }
            );
        }
    );

    EXPECT_EQ(0, server_receive_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    ASSERT_EQ(2, server_receive_counter);
}

TEST_F(UdpClientServerTest, peer_identity_with_preservation_on_server) {
    io::EventLoop loop;

    const std::string client_message_1 = "message_1";
    const std::string client_message_2 = "close";

    std::size_t server_receive_counter = 0;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);

        if (server_receive_counter == 0) {
            peer.set_user_data(&server_receive_counter);
        } else {
            EXPECT_EQ(&server_receive_counter, peer.user_data());
        }

        ++server_receive_counter;

        std::string s(data.buf.get(), data.size);
        if (s == client_message_2) {
            server->schedule_removal();
        }
    },
    1000,
    nullptr);

    EXPECT_FALSE(listen_error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({0x7F000001u, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            client.send_data(client_message_1,
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    client.send_data(client_message_2,
                        [&](io::net::UdpClient& client, const io::Error& error) {
                            EXPECT_FALSE(error);
                            client.schedule_removal();
                        }
                    );
                }
            );
        }
    );

    EXPECT_EQ(0, server_receive_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    ASSERT_EQ(2, server_receive_counter);
}

TEST_F(UdpClientServerTest, on_new_peer_callback) {
    io::EventLoop loop;

    std::size_t on_new_peer_counter = 0;
    std::size_t on_data_receive_counter = 0;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer& peer, const io::Error& error) {
            EXPECT_FALSE(error);

            if (!on_new_peer_counter) {
                EXPECT_EQ(0, on_data_receive_counter);
            }

            ++on_new_peer_counter;
        },
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++on_data_receive_counter;

            if (on_data_receive_counter == 4) {
                server->schedule_removal();
            }
        },
    100,
    nullptr);

    auto client_1 = new io::net::UdpClient(loop);
    client_1->set_destination({0x7F000001u, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            client.send_data("1_1",
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    client.send_data("1_2",
                        [&](io::net::UdpClient& client, const io::Error& error) {
                            EXPECT_FALSE(error);
                            client.schedule_removal();
                        }
                    );
                }
            );
        }
    );

    auto client_2 = new io::net::UdpClient(loop);
    client_2->set_destination({0x7F000001u, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            client.send_data("2_1",
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    client.send_data("2_2",
                        [&](io::net::UdpClient& client, const io::Error& error) {
                            EXPECT_FALSE(error);
                            client.schedule_removal();
                        }
                    );
                }
            );
        }
    );

    EXPECT_EQ(0, on_new_peer_counter);
    EXPECT_EQ(0, on_data_receive_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(2, on_new_peer_counter);
    EXPECT_EQ(4, on_data_receive_counter);
}

TEST_F(UdpClientServerTest, client_timeout_for_server) {
    // Note: timings are essential in this test
    // Timeout : 200ms
    // UdpClient sends:    0 - 100 - - - - - - - - - - - - - 600
    //                          |-----| <- interval less than timeout. Decision: keep
    //                          |-----------------| <- interval more than timeout. Decision: drop client
    //                                                        | <- register previos client as new one at this point

    io::EventLoop loop;

    const std::string client_message_1 = "message_1";
    const std::string client_message_2 = "message_2";
    const std::string client_message_3 = "message_3";

    std::size_t server_receive_counter = 0;
    std::size_t server_timeout_counter = 0;
    std::size_t client_send_counter = 0;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);

        if (server_receive_counter == 0) {
            peer.set_user_data(&server_receive_counter);
        } else if (server_receive_counter == 1) {
            EXPECT_EQ(&server_receive_counter, peer.user_data());
        } else {
            EXPECT_FALSE(peer.user_data());
        }

        ++server_receive_counter;

        std::string s(data.buf.get(), data.size);
        if (s == client_message_3) {
            server->schedule_removal();
        }
    },
    200, // timeout MS
    [&](io::net::UdpPeer& client, const io::Error& error) {
        EXPECT_FALSE(error);

        ASSERT_TRUE(client.user_data());
        auto& value = *reinterpret_cast<std::size_t*>(client.user_data());
        EXPECT_EQ(2, value);

        ++server_timeout_counter;
    });
    ASSERT_FALSE(listen_error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({0x7F000001u, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            client.send_data(client_message_1,
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    ++client_send_counter;
                }
            );
        }
    );

    auto timer_1 = new io::Timer(loop);
    timer_1->start(100, [&](io::Timer& timer) {
        EXPECT_EQ(1, client_send_counter);
        client->send_data(client_message_2,
            [&](io::net::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error) << error;
                ++client_send_counter;
            }
        );
        timer.schedule_removal();
    });

    auto timer_2 = new io::Timer(loop);
    timer_2->start(600, [&](io::Timer& timer) {
        EXPECT_EQ(2, client_send_counter);
        client->send_data(client_message_3,
            [&](io::net::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error) << error;
                ++client_send_counter;
                client.schedule_removal();
                timer.schedule_removal();
            }
        );
    });

    EXPECT_EQ(0, client_send_counter);
    EXPECT_EQ(0, server_receive_counter);
    EXPECT_EQ(0, server_timeout_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    ASSERT_EQ(3, client_send_counter);
    ASSERT_EQ(3, server_receive_counter);
    EXPECT_EQ(1, server_timeout_counter);
}

TEST_F(UdpClientServerTest, multiple_clients_timeout_for_server) {
    // Note: timings are essential in this test
    // Timeout : 440ms
    // UdpClient1 send:    0 - 200 - 400 - 600 - 800 -1000 -1200 | - - - - - - - x -
    // UdpClient2 send:    0 - - - - 400 - - - - 800 - - - -1200 | - - - - - - - x -
    // UdpClient3 send:    0 - - - - - x - - - - 800 - - - - - x | - - - - - - - - -
    //                                                           | <- stop sending here
    //                                                         terminate there -> ...
    // 'x' - is client timeout

    io::EventLoop loop;

    std::string client_1_message = "client_1";
    std::string client_2_message = "client_2";
    std::string client_3_message = "client_3";

    std::unordered_map<std::string, std::size_t> peer_to_close_count;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::net::UdpPeer& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);

        std::string s(data.buf.get(), data.size);
        if (s == client_1_message) {
            client.set_user_data(&client_1_message);
        } else if (s == client_2_message) {
            client.set_user_data(&client_2_message);
        } else if (s == client_3_message) {
            client.set_user_data(&client_3_message);
        }
    },
    440, // timeout MS
    [&](io::net::UdpPeer& client, const io::Error& error) {
        EXPECT_FALSE(error);
        ASSERT_TRUE(client.user_data());

        const auto& data_str = *reinterpret_cast<std::string*>(client.user_data());
        peer_to_close_count[data_str]++;
    });
    ASSERT_FALSE(listen_error) << listen_error;

    auto timer_1 = new io::Timer(loop);
    auto client_1 = new io::net::UdpClient(loop);
    client_1->set_destination({0x7F000001u, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            timer_1->start(0, 200, [&](io::Timer& timer) {
                client_1->send_data(client_1_message,
                    [&](io::net::UdpClient& client, const io::Error& error) {
                        EXPECT_FALSE(error) << error;
                    }
                );
            });
        }
    );

    auto timer_2 = new io::Timer(loop);
    auto client_2 = new io::net::UdpClient(loop);
    client_2->set_destination({0x7F000001u, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            timer_2->start(0, 400, [&](io::Timer& timer) {
                client_2->send_data(client_2_message,
                    [&](io::net::UdpClient& client, const io::Error& error) {
                        EXPECT_FALSE(error);
                    }
                );
            });
        }
    );

    auto timer_3 = new io::Timer(loop);
    auto client_3 = new io::net::UdpClient(loop);
    client_3->set_destination({0x7F000001u, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            timer_3->start(0, 800, [&](io::Timer& timer) {
                client_3->send_data(client_3_message,
                    [&](io::net::UdpClient& client, const io::Error& error) {
                        EXPECT_FALSE(error);
                    }
                );
            });
        }
    );

    auto timer_stop_all = new io::Timer(loop);
    timer_stop_all->start(1300, [&](io::Timer& timer) {
        timer_1->stop();
        timer_2->stop();
        timer_3->stop();
        timer.schedule_removal();
    });

    auto timer_remove = new io::Timer(loop);
    timer_remove->start(2000, [&](io::Timer& timer) {
        client_1->schedule_removal();
        client_2->schedule_removal();
        client_3->schedule_removal();
        server->schedule_removal();

        timer_1->schedule_removal();
        timer_2->schedule_removal();
        timer_3->schedule_removal();
        timer.schedule_removal();
    });

    EXPECT_EQ(0, peer_to_close_count.size());

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(3, peer_to_close_count.size());
    EXPECT_EQ(1, peer_to_close_count[client_1_message]);
    EXPECT_EQ(1, peer_to_close_count[client_2_message]);
    EXPECT_EQ(2, peer_to_close_count[client_3_message]);
}

TEST_F(UdpClientServerTest, peer_is_not_expired_while_sends_data) {
    io::EventLoop loop;

    const std::deque<std::uint64_t> send_timeouts = {100, 100, 100, 100, 100, 100};
    const std::size_t TIMEOUT_MS = 400;
    const std::size_t EXPECTED_ELAPSED_TIME_MS = 600 + TIMEOUT_MS;

    std::size_t peer_timeout_counter = 0;
    std::size_t server_data_send_counter = 0;
    std::size_t client_data_receive_counter = 0;

    const auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            auto timer = new io::Timer(loop);
            timer->start(
                send_timeouts,
                [&](io::Timer& timer) {
                    ++server_data_send_counter;
                    client.send_data("!!!");
                    if (server_data_send_counter == send_timeouts.size()) {
                        timer.schedule_removal();
                    }
                }
            );
        },
        TIMEOUT_MS,
        [&](io::net::UdpPeer& client, const io::Error& error) {
            ++peer_timeout_counter;
            EXPECT_FALSE(error);
            t2 = std::chrono::high_resolution_clock::now();

            EXPECT_TIMEOUT_MS(EXPECTED_ELAPSED_TIME_MS,
                              std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1));

            server->schedule_removal();
        }
    );

    auto client = new io::net::UdpClient(loop);
    client->set_destination({0x7F000001u, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data("!!!");
        },
        [&](io::net::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            ++client_data_receive_counter;

            if (client_data_receive_counter == send_timeouts.size()) {
                client.schedule_removal();
            }
        }
    );

    EXPECT_EQ(0, server_data_send_counter);
    EXPECT_EQ(0, client_data_receive_counter);
    EXPECT_EQ(0, peer_timeout_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(send_timeouts.size(), server_data_send_counter);
    EXPECT_EQ(send_timeouts.size(), client_data_receive_counter);
    EXPECT_EQ(1, peer_timeout_counter);
}

TEST_F(UdpClientServerTest, server_close) {
    io::EventLoop loop;

    std::size_t server_peer_timeout_call_count = 0;

    std::size_t server_close_1_callback_call_count = 0;
    std::size_t server_close_2_callback_call_count = 0;

    auto server = new io::net::UdpServer(loop);
    server->close(
        [&](io::net::UdpServer& server, const io::Error& error) {
            ++server_close_1_callback_call_count; // Should not be called
        }
    );

    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer& peer, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        10000,
        [&](io::net::UdpPeer& peer, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_peer_timeout_call_count;
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data("!!!",
                [](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string();
                    client.schedule_removal();
                }
            );
        }
    );

    server->close(
        [&](io::net::UdpServer& server, const io::Error& error) {
            ++server_close_2_callback_call_count;
            server.close();
            server.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_peer_timeout_call_count);
    EXPECT_EQ(0, server_close_1_callback_call_count);
    EXPECT_EQ(0, server_close_2_callback_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(0, server_peer_timeout_call_count);
    EXPECT_EQ(0, server_close_1_callback_call_count);
    EXPECT_EQ(1, server_close_2_callback_call_count);
}

TEST_F(UdpClientServerTest, server_schedule_removal_in_server_close_callback) {
    io::EventLoop loop;

    std::size_t server_close_callback_call_count = 0;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        }
    );
    ASSERT_FALSE(listen_error);

    server->close(
        [&](io::net::UdpServer& server, const io::Error& error) {
            ++server_close_callback_call_count; // Should not be called
            server.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_close_callback_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_close_callback_call_count);
}

TEST_F(UdpClientServerTest, client_and_server_send_data_each_other) {
    io::EventLoop loop;

    const std::string client_message = "Hello from client!";
    const std::string server_message = "Hello from server!";

    std::size_t server_data_receive_counter = 0;
    std::size_t server_data_send_counter = 0;

    std::size_t client_data_receive_counter = 0;
    std::size_t client_data_send_counter = 0;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        ++server_data_receive_counter;

        std::string s(data.buf.get(), data.size);
        EXPECT_EQ(client_message, s);

        peer.send_data(server_message,
            [&](io::net::UdpPeer& peer, const io::Error& error) {
                EXPECT_FALSE(error) << error.string();
                ++server_data_send_counter;
                server->schedule_removal();
            }
        );
    });
    ASSERT_FALSE(listen_error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data(client_message,
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    ++client_data_send_counter;
                }
            );
        },
        [&](io::net::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            std::string s(data.buf.get(), data.size);
            EXPECT_EQ(server_message, s);

            ++client_data_receive_counter;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_data_receive_counter);
    EXPECT_EQ(0, server_data_send_counter);
    EXPECT_EQ(0, client_data_receive_counter);
    EXPECT_EQ(0, client_data_send_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_data_receive_counter);
    EXPECT_EQ(1, server_data_send_counter);
    EXPECT_EQ(1, client_data_receive_counter);
    EXPECT_EQ(1, client_data_send_counter);
}

TEST_F(UdpClientServerTest, client_and_server_use_char_buffer_for_send) {
    io::EventLoop loop;

    const char client_message[] = "Hello from client!";
    const char server_message[] = "Hello from server!";

    std::size_t server_data_receive_counter = 0;
    std::size_t server_data_send_counter = 0;

    std::size_t client_data_receive_counter = 0;
    std::size_t client_data_send_counter = 0;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        ++server_data_receive_counter;

        std::string s(data.buf.get(), data.size);
        EXPECT_EQ(client_message, s);

        peer.send_data(server_message, sizeof(server_message) - 1, // -1 is for last 0
            [&](io::net::UdpPeer& peer, const io::Error& error) {
                EXPECT_FALSE(error) << error.string();
                ++server_data_send_counter;
                server->schedule_removal();
            }
        );
    });
    ASSERT_FALSE(listen_error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data(client_message, sizeof(client_message) - 1,  // -1 is for last 0
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    ++client_data_send_counter;
                }
            );
        },
        [&](io::net::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            std::string s(data.buf.get(), data.size);
            EXPECT_EQ(server_message, s);

            ++client_data_receive_counter;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_data_receive_counter);
    EXPECT_EQ(0, server_data_send_counter);
    EXPECT_EQ(0, client_data_receive_counter);
    EXPECT_EQ(0, client_data_send_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_data_receive_counter);
    EXPECT_EQ(1, server_data_send_counter);
    EXPECT_EQ(1, client_data_receive_counter);
    EXPECT_EQ(1, client_data_send_counter);
}

TEST_F(UdpClientServerTest, null_server_send_buf) {
    io::EventLoop loop;

    std::size_t client_on_send_count = 0;
    std::size_t server_on_send_count = 0;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            peer.send_data(nullptr, 0,
                [&](io::net::UdpPeer& peer, const io::Error& error) {
                    EXPECT_TRUE(error);
                    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
                    ++server_on_send_count;
                    server->schedule_removal();
                }
            );
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data("!!!", // Just to force server to reply with nullptr message
                [&](io::net::UdpClient& client, const io::Error& error) {
                    client.send_data(nullptr, 0,
                        [&](io::net::UdpClient& client, const io::Error& error) {
                            EXPECT_TRUE(error);
                            EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
                            ++client_on_send_count;
                            client.schedule_removal();
                        }
                    );
                }
            );
        }
    );

    EXPECT_EQ(0, client_on_send_count);
    EXPECT_EQ(0, server_on_send_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_send_count);
    EXPECT_EQ(1, server_on_send_count);
}

TEST_F(UdpClientServerTest, server_reply_with_2_messages) {
    // Test description: here checking that UdpPeer in a server state without peer timeout enabled
    // allows to send data correctly and no memory corruption happens
    io::EventLoop loop;

    const std::string client_message = "Hello from client!";
    const std::string server_message[2] = {
        "Hello from server 1!",
        "Hello from server 2!"
    };

    std::size_t server_data_receive_counter = 0;
    std::size_t server_data_send_counter = 0;

    std::size_t client_data_receive_counter = 0;
    std::size_t client_data_send_counter = 0;

    auto server = new io::net::UdpServer(loop);

    std::function<void(io::net::UdpPeer&, const io::Error&)> on_server_send =
        [&](io::net::UdpPeer& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_send_counter;

            if (server_data_send_counter < 2) {
                client.send_data(server_message[server_data_send_counter], on_server_send);
            } else {
                server->schedule_removal();
            }
        };

    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);

        ++server_data_receive_counter;
        peer.send_data(server_message[server_data_send_counter], on_server_send);
    });

    EXPECT_FALSE(listen_error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data(client_message,
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    ++client_data_send_counter;
                }
            );
        },
        [&](io::net::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            std::string s(data.buf.get(), data.size);
            EXPECT_EQ(server_message[client_data_receive_counter], s);

            ++client_data_receive_counter;

            if (client_data_receive_counter == 2) {
                client.schedule_removal();
            }
        }
    );

    EXPECT_EQ(0, server_data_receive_counter);
    EXPECT_EQ(0, server_data_send_counter);
    EXPECT_EQ(0, client_data_receive_counter);
    EXPECT_EQ(0, client_data_send_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_data_receive_counter);
    EXPECT_EQ(2, server_data_send_counter);
    EXPECT_EQ(2, client_data_receive_counter);
    EXPECT_EQ(1, client_data_send_counter);
}

TEST_F(UdpClientServerTest, client_receive_data_only_from_its_target) {
    io::EventLoop loop;

    const std::string client_message = "I am client";
    const std::string other_message = "Spam!Spam!Spam!";

    std::size_t receive_callback_call_count = 0;

    auto client_2 = new io::net::UdpClient(loop);

    auto client_1 = new io::net::UdpClient(loop);
    client_1->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            EXPECT_NE(0, client_1->bound_port());
            // Setting destination with client_1 bound port to simulate response from outside
            client_2->set_destination({m_default_addr, client_1->bound_port()},
               [&](io::net::UdpClient& client, const io::Error& error) {
                   EXPECT_FALSE(error) << error;
                   client.send_data(other_message,
                       [&](io::net::UdpClient& client, const io::Error& error) {
                           EXPECT_FALSE(error) << error;
                       }
                    );
                }
            );
        },
        [&](io::net::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++receive_callback_call_count;
        }
    );

    EXPECT_EQ(0, client_1->bound_port());

    auto timer = new io::Timer(loop);
    timer->start(200,
        [&](io::Timer& timer) {
            client_1->schedule_removal();
            client_2->schedule_removal();
            timer.schedule_removal();
        }
    );

    EXPECT_EQ(0, receive_callback_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(0, receive_callback_call_count);
}

TEST_F(UdpClientServerTest, send_larger_than_ethernet_mtu) {
    io::EventLoop loop;

    std::size_t SIZE = 5000;

    bool data_sent = false;
    bool data_received = false;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        EXPECT_EQ(SIZE, data.size);
        data_received = true;

        for (size_t i = 0; i < SIZE / 2; ++i) {
            ASSERT_EQ(i, *(reinterpret_cast<const std::uint16_t*>(data.buf.get()) + i))  << "i =" << i;
        }

        server->schedule_removal();
    });

    EXPECT_FALSE(listen_error);

    std::shared_ptr<char> message(new char[SIZE], std::default_delete<char[]>());
    for (size_t i = 0; i < SIZE / 2; ++i) {
        *(reinterpret_cast<std::uint16_t*>(message.get()) + i) = i;
    }

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data(message, SIZE,
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    data_sent = true;
                    client.schedule_removal();
                }
            );
        }
    );

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_TRUE(data_sent);
    EXPECT_TRUE(data_received);
}

TEST_F(UdpClientServerTest, send_larger_than_allowed_to_send) {
    io::EventLoop loop;

    std::size_t client_on_send_call_count = 0;

    std::size_t SIZE = 100 * 1024;

    std::shared_ptr<char> message(new char[SIZE], std::default_delete<char[]>());
    std::memset(message.get(), 0, SIZE);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data(message, SIZE,
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_TRUE(error);
                    EXPECT_EQ(io::StatusCode::MESSAGE_TOO_LONG, error.code());
                    ++client_on_send_call_count;
                    client.schedule_removal();
                }
            );
        }
    );

    EXPECT_EQ(0, client_on_send_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_send_call_count);
}

TEST_F(UdpClientServerTest, client_and_server_exchange_lot_of_packets) {
    // Note: as we perform this test on local host on the same event loop, we expect that data
    // will be received sequentially
    io::EventLoop loop;

    std::size_t SIZE = 2000;
    std::shared_ptr<char> message(new char[SIZE], std::default_delete<char[]>());
    ::srand(0);
    for(std::size_t i = 0; i < SIZE; ++i) {
        message.get()[i] = ::rand() & 0xFF;
    }

    std::size_t server_send_message_counter = 0;
    std::size_t server_receive_message_counter = 0;
    std::size_t client_send_message_counter = 0;
    std::size_t client_receive_message_counter = 0;
    bool server_send_started = false;

    std::function<void(io::net::UdpPeer&, const io::Error&)> server_send =
        [&](io::net::UdpPeer& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_send_message_counter;
            if (server_send_message_counter < SIZE) {
                client.send_data(message, SIZE - server_send_message_counter, server_send);
            }
        };

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer& client, const io::DataChunk& chunk, const io::Error& error) {
            EXPECT_FALSE(error);

            for (std::size_t i = 0; i < SIZE - server_receive_message_counter; ++i) {
                ASSERT_EQ(message.get()[i], chunk.buf.get()[i]) << "i= " << i;
            }

            if (!server_send_started) {
                client.send_data(message, SIZE - server_receive_message_counter, server_send);
                server_send_started = true;
            }

            ++server_receive_message_counter;
        }
    );

    EXPECT_FALSE(listen_error);

    std::function<void(io::net::UdpClient&, const io::Error&)> client_send =
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_send_message_counter;
            if (client_send_message_counter < SIZE) {
                client.send_data(message, SIZE - client_send_message_counter, client_send);
            }
        };

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data(message, SIZE - client_send_message_counter, client_send);
        },
        [&](io::net::UdpClient&, const io::DataChunk& chunk, const io::Error& error) {
            EXPECT_FALSE(error);
            ASSERT_EQ(SIZE - client_receive_message_counter, chunk.size);
            for (std::size_t i = 0; i < SIZE - client_receive_message_counter; ++i) {
                ASSERT_EQ(message.get()[i], chunk.buf.get()[i]) << "i= " << i;
            }

            ++client_receive_message_counter;
        }
    );

    auto timer = new io::Timer(loop);
    timer->start(100, 100,
        [&](io::Timer& timer) {
            if (client_send_message_counter == SIZE && server_send_message_counter == SIZE) {
                client->schedule_removal();
                server->schedule_removal();
                timer.schedule_removal();
            }
        }
    );

    EXPECT_EQ(0, server_receive_message_counter);
    EXPECT_EQ(0, client_receive_message_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(SIZE, server_receive_message_counter);
    EXPECT_EQ(SIZE, client_receive_message_counter);
}

// DOC: explain that too much of UDP sending may result in lost packets
//      need to configure networking stack on some particular machine.
//      on Linux 'net.core.wmem_default', 'net.core.rmem_max' and so on...

// From man 2 sendmsg

// ENOBUFS
// The output queue for a network interface was full.  This generally indicates that the interface has stopped sending, but may  be
// caused by transient congestion.  (Normally, this does not occur in Linux.  Packets are just silently dropped when a device queue
// overflows.)

TEST_F(UdpClientServerTest, client_and_server_exchange_lot_of_packets_in_threads) {
    const std::size_t SERVER_RECEIVE_BUFFER_SIZE = 128 * 1024;
    if (SERVER_RECEIVE_BUFFER_SIZE > io::global::max_receive_buffer_size()) {
        IO_TEST_SKIP();
    }

    const std::size_t SIZE = 200;
    std::shared_ptr<char> message(new char[SIZE], std::default_delete<char[]>());
    ::srand(0);
    for(std::size_t i = 0; i < SIZE; ++i) {
        message.get()[i] = ::rand() & 0xFF;
    }

    std::size_t server_receive_message_counter = 0;
    std::size_t client_receive_message_counter = 0;

    std::thread server_thread([&]() {
        bool server_send_started = false;

        std::size_t server_send_message_counter = 0;
        std::function<void(io::net::UdpPeer&, const io::Error&)> on_server_send =
            [&](io::net::UdpPeer& client, const io::Error& error) {
                EXPECT_FALSE(error);

                ++server_send_message_counter;
                if (server_send_message_counter < SIZE) {
                    client.send_data(message, SIZE - server_send_message_counter, on_server_send);
                }
            };

        io::EventLoop server_loop;

        auto server = new io::net::UdpServer(server_loop);
        auto listen_error = server->start_receive({m_default_addr, m_default_port},
            [&](io::net::UdpPeer& client, const io::DataChunk& chunk, const io::Error& error) {
                EXPECT_FALSE(error);

                for (std::size_t i = 0; i < chunk.size; ++i) {
                    ASSERT_EQ(message.get()[i], chunk.buf.get()[i]) << "i= " << i;
                }

                if (!server_send_started) {
                    client.send_data(message, SIZE - chunk.size + 1, on_server_send);
                    server_send_started = true;
                }

                ++server_receive_message_counter;
            }
        );
        EXPECT_FALSE(listen_error);
		EXPECT_FALSE(server->set_receive_buffer_size(SERVER_RECEIVE_BUFFER_SIZE));

        auto timer = new io::Timer(server_loop);
        timer->start(100, 100,
            [&](io::Timer& timer) {
                //std::cout << "s " << server_receive_message_counter << " " << server_send_message_counter << std::endl;
                if (server_receive_message_counter == SIZE && server_send_message_counter == SIZE) {
                    server->schedule_removal();
                    timer.schedule_removal();
                }
            }
        );

        EXPECT_EQ(0, server_receive_message_counter);

        ASSERT_EQ(io::StatusCode::OK, server_loop.run());

        EXPECT_EQ(SIZE, server_receive_message_counter);
    });

    std::thread client_thread([&]() {
        // Give a time to server thread to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::size_t client_send_message_counter = 0;
        std::function<void(io::net::UdpClient&, const io::Error&)> client_send =
            [&](io::net::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);

                ++client_send_message_counter;
                if (client_send_message_counter < SIZE) {
                    client.send_data(message, SIZE - client_send_message_counter, client_send);
                }
            };

        io::EventLoop client_loop;

        auto client = new io::net::UdpClient(client_loop);
        client->set_destination({m_default_addr, m_default_port},
            [&](io::net::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error) << error;
                client.send_data(message, SIZE, client_send);
            },
            [&](io::net::UdpClient& client, const io::DataChunk& chunk, const io::Error& error) {
                EXPECT_FALSE(error);
                for (std::size_t i = 0; i < chunk.size; ++i) {
                    ASSERT_EQ(message.get()[i], chunk.buf.get()[i]) << "i= " << i;
                }

                ++client_receive_message_counter;
            }
        );

        auto timer = new io::Timer(client_loop);
        timer->start(100, 100,
            [&](io::Timer& timer) {
                if (client_receive_message_counter == SIZE && client_send_message_counter == SIZE) {
                    client->schedule_removal();
                    timer.schedule_removal();
                }
            }
        );

        EXPECT_EQ(0, client_receive_message_counter);

        ASSERT_EQ(io::StatusCode::OK, client_loop.run());

        EXPECT_EQ(SIZE, client_receive_message_counter);
    });

    server_thread.join();
    client_thread.join();
}

TEST_F(UdpClientServerTest, send_after_schedule_removal) {
    io::EventLoop loop;

    std::size_t server_receive_counter = 0;
    std::size_t client_send_counter = 0;

    auto server = new io::net::UdpServer(loop);
    io::Error listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_receive_counter;
        }
    );
    EXPECT_FALSE(listen_error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.schedule_removal();
            client.send_data("Hello",
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_TRUE(error);
                    EXPECT_EQ(io::StatusCode::OPERATION_CANCELED, error.code());
                    ++client_send_counter;
                    server->schedule_removal();
                }
            );
        }
    );

    EXPECT_EQ(0, server_receive_counter);
    EXPECT_EQ(0, client_send_counter);

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(0, server_receive_counter);
    EXPECT_EQ(1, client_send_counter);
}

TEST_F(UdpClientServerTest, client_with_timeout_1) {
    io::EventLoop loop;

    const std::size_t TIMEOUT = 500;

    const auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            EXPECT_NE(0, client.bound_port());
        },
        [&](io::net::UdpClient& client, const io::DataChunk& chunk, const io::Error& error) {
        },
        TIMEOUT,
        [&](io::net::UdpClient& client, const io::Error& error) {
            client.schedule_removal();
            t2 = std::chrono::high_resolution_clock::now();
        }
    );

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_TIMEOUT_MS(TIMEOUT, std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1));
}

TEST_F(UdpClientServerTest, client_with_timeout_2) {
    // Note: testing that after timeout data is not sent anymore
    io::EventLoop loop;

    std::size_t server_receive_counter = 0;
    std::size_t client_send_counter = 0;

    auto server = new io::net::UdpServer(loop);
    io::Error listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_receive_counter;
        }
    );
    EXPECT_FALSE(listen_error);

    const std::size_t TIMEOUT = 200;

    const auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            EXPECT_NE(0, client.bound_port());
        },
        [&](io::net::UdpClient& client, const io::DataChunk& chunk, const io::Error& error) {
        },
        TIMEOUT,
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_EQ(0, client.bound_port());

            t2 = std::chrono::high_resolution_clock::now();
            EXPECT_TIMEOUT_MS(TIMEOUT, std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
            client.send_data("!!!",
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_TRUE(error);
                    EXPECT_EQ(io::StatusCode::OPERATION_CANCELED, error.code());
                    ++client_send_counter;

                    auto timer = new io::Timer(loop);
                    timer->start(50, [&](io::Timer& timer) {
                        client.schedule_removal();
                        server->schedule_removal();
                        timer.schedule_removal();
                    });
                }
            );
        }
    );

    EXPECT_EQ(0, client->bound_port());

    EXPECT_EQ(0, server_receive_counter);
    EXPECT_EQ(0, client_send_counter);

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(0, server_receive_counter);
    EXPECT_EQ(1, client_send_counter);

    EXPECT_TIMEOUT_MS(TIMEOUT, std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1));
}

TEST_F(UdpClientServerTest, client_with_timeout_3) {
    // Note: testing that receiving data will delay peer timeout (prolong lifetime)

    io::EventLoop loop;

    const std::deque<std::uint64_t> send_timeouts = { 100, 100, 100, 100, 100, 100 };
    const auto min_sum_send_timeout = std::accumulate(send_timeouts.begin(), send_timeouts.end(), 0);

    const std::size_t CLIENT_TIMEOUT = 200;
    const std::size_t EXPECTED_ELAPSED_TIME = CLIENT_TIMEOUT + min_sum_send_timeout;

    std::size_t client_on_timeout_count = 0;
    std::size_t client_send_counter = 0;

    // As send timers most likely send data later than expected, we need to count overdue to compensate
    // expected peer timeout time overdue on server.
    std::chrono::milliseconds send_timers_overdue(0);

    const auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        nullptr,
        nullptr,
        CLIENT_TIMEOUT,
        [&](io::net::UdpClient& client, const io::Error& error) {
            ++client_on_timeout_count;

            t2 = std::chrono::high_resolution_clock::now();
            EXPECT_TIMEOUT_MS(EXPECTED_ELAPSED_TIME,
                              std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() - send_timers_overdue.count());

            client.schedule_removal();
        }
    );

    auto timer = new io::Timer(loop);
    timer->start(
        send_timeouts,
        [&](io::Timer& timer) {
            send_timers_overdue += timer.real_time_passed_since_last_callback() - std::chrono::milliseconds(timer.timeout_ms());
            ASSERT_EQ(0, client_on_timeout_count);
            client->send_data("!!!",
            [&](io::net::UdpClient& client, const io::Error& error) {
                if (++client_send_counter == send_timeouts.size()) {
                    timer.schedule_removal();
                }
            });
        }
    );

    EXPECT_EQ(0, client_on_timeout_count);
    EXPECT_EQ(0, client_send_counter);

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_timeout_count);
    EXPECT_EQ(send_timeouts.size(), client_send_counter);
}

TEST_F(UdpClientServerTest, close_peer_from_server) {
    // Note: UDP peer inactivity timeout test (similar to TIME_WAIT for TCP)
    io::EventLoop loop;

    const std::size_t INACTIVE_TIMEOUT_MS = 500;

    std::size_t server_on_data_receive_callback_count = 0;
    std::size_t server_on_new_peer_callback_count = 0;
    std::size_t server_on_peer_timeout_callback_count = 0;

    std::chrono::high_resolution_clock::time_point t1;
    std::chrono::high_resolution_clock::time_point t2;

    auto server = new io::net::UdpServer(loop);
    auto error = server->start_receive(
        { m_default_addr, m_default_port } ,
        [&] (io::net::UdpPeer&, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_new_peer_callback_count;
        },
        [&] (io::net::UdpPeer& peer, const io::DataChunk&, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            EXPECT_LE(server_on_data_receive_callback_count, 2);

            peer.close(INACTIVE_TIMEOUT_MS);

            if (server_on_data_receive_callback_count == 0) {
                t1 = std::chrono::high_resolution_clock::now();
            } else if (server_on_data_receive_callback_count == 1) {
                t2 = std::chrono::high_resolution_clock::now();
            }

            ++server_on_data_receive_callback_count;
        },
        100 * 1000,
        [&] (io::net::UdpPeer&, const io::Error&){
            ++server_on_peer_timeout_callback_count;
        }
    );
    EXPECT_FALSE(error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            auto cleint_send_timer = new io::Timer(loop);
            cleint_send_timer->start(
                1,
                1,
                [&](io::Timer& timer) {
                    client.send_data("!");
                    if (server_on_data_receive_callback_count == 2) {
                        server->schedule_removal();
                        timer.schedule_removal();
                        client.schedule_removal();
                    }
                }
            );
        }
    );

    EXPECT_EQ(0, server_on_data_receive_callback_count);
    EXPECT_EQ(0, server_on_new_peer_callback_count);
    EXPECT_EQ(0, server_on_peer_timeout_callback_count);

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(2, server_on_data_receive_callback_count);
    EXPECT_EQ(2, server_on_new_peer_callback_count);
    EXPECT_EQ(0, server_on_peer_timeout_callback_count);

    const auto time_between_received_packets = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    EXPECT_TIMEOUT_MS(INACTIVE_TIMEOUT_MS, time_between_received_packets);
}

TEST_F(UdpClientServerTest, closed_peer_from_server_has_no_timeout) {
    io::EventLoop loop;

    const std::size_t INACTIVE_TIMEOUT = 250;
    const std::size_t CLIENT_TIMEOUT = 100;

    std::size_t server_on_data_receive_callback_count = 0;
    std::size_t server_on_new_peer_callback_count = 0;
    std::size_t server_on_peer_timeout_callback_count = 0;

    auto server = new io::net::UdpServer(loop);
    auto error = server->start_receive(
        { m_default_addr, m_default_port } ,
        [&] (io::net::UdpPeer&, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_new_peer_callback_count;
        },
        [&] (io::net::UdpPeer& peer, const io::DataChunk&, const io::Error&) {
            peer.close(INACTIVE_TIMEOUT);

            ++server_on_data_receive_callback_count;
        },
        CLIENT_TIMEOUT,
        [&] (io::net::UdpPeer&, const io::Error&){
            ++server_on_peer_timeout_callback_count;
        }
    );
    EXPECT_FALSE(error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data("!");

            auto cleint_send_timer = new io::Timer(loop);
            cleint_send_timer->start(
                INACTIVE_TIMEOUT * 2,
                [&](io::Timer& timer) {
                    server->schedule_removal();
                    timer.schedule_removal();
                    client.schedule_removal();
                }
            );
        }
    );

    EXPECT_EQ(0, server_on_data_receive_callback_count);
    EXPECT_EQ(0, server_on_new_peer_callback_count);
    EXPECT_EQ(0, server_on_peer_timeout_callback_count);

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_data_receive_callback_count);
    EXPECT_EQ(1, server_on_new_peer_callback_count);
    EXPECT_EQ(0, server_on_peer_timeout_callback_count);
}

TEST_F(UdpClientServerTest, double_close_peer_from_server) {
    // Note: testing that second attempt to close peer while its being closed has no efect

    io::EventLoop loop;

    const std::size_t INACTIVE_TIMEOUT_1 = 200;
    const std::size_t INACTIVE_TIMEOUT_2 = 50;

    std::size_t server_on_data_receive_callback_count = 0;

    auto server = new io::net::UdpServer(loop);
    auto error = server->start_receive(
        { m_default_addr, m_default_port } ,
        [&] (io::net::UdpPeer& peer, const io::DataChunk&, const io::Error&) {
            peer.close(INACTIVE_TIMEOUT_1);
            peer.close(INACTIVE_TIMEOUT_2); // This will be ignored

            ++server_on_data_receive_callback_count;
        },
        1000,
        nullptr
    );
    EXPECT_FALSE(error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data("!");

            auto cleint_send_timer = new io::Timer(loop);
            cleint_send_timer->start(
                INACTIVE_TIMEOUT_2 * 2,
                [&] (io::Timer& timer) {
                    client.send_data("!");
                    timer.schedule_removal();
                }
            );
        }
    );

    auto cleanup_timer = new io::Timer(loop);
    cleanup_timer->start(
        INACTIVE_TIMEOUT_2 * 3,
        [&] (io::Timer& timer) {
            server->schedule_removal();
            client->schedule_removal();
            timer.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_on_data_receive_callback_count);

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_data_receive_callback_count);
}

TEST_F(UdpClientServerTest, close_peer_from_server_and_than_try_send) {
    io::EventLoop loop;

    const std::size_t INACTIVE_TIMEOUT = 100;

    std::size_t server_on_data_receive_callback_count = 0;
    std::size_t peer_on_send_callback_count = 0;

    auto server = new io::net::UdpServer(loop);
    auto error = server->start_receive(
        { m_default_addr, m_default_port } ,
        [&] (io::net::UdpPeer& peer, const io::DataChunk&, const io::Error&) {
            peer.close(INACTIVE_TIMEOUT);

            ++server_on_data_receive_callback_count;

            peer.send_data("!",
                [&](io::net::UdpPeer& peer, const io::Error& error) {
                    EXPECT_TRUE(error);
                    ++peer_on_send_callback_count;
                }
            );
        },
        1000 * 5,
        nullptr
    );
    EXPECT_FALSE(error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data("!");
        }
    );

    auto cleanup_timer = new io::Timer(loop);
    cleanup_timer->start(
        INACTIVE_TIMEOUT * 2,
        [&] (io::Timer& timer) {
            server->schedule_removal();
            timer.schedule_removal();
            client->schedule_removal();
        }
    );

    EXPECT_EQ(0, server_on_data_receive_callback_count);
    EXPECT_EQ(0, peer_on_send_callback_count);

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_data_receive_callback_count);
    EXPECT_EQ(1, peer_on_send_callback_count);
}

TEST_F(UdpClientServerTest, send_data_of_size_0) {
    io::EventLoop loop;

    std::size_t server_on_send_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t client_on_send_count = 0;
    std::size_t client_on_receive_count = 0;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({"0.0.0.0", m_default_port},
        [&](io::net::UdpPeer& client, const io::DataChunk& data, const io::Error& error) {
            ++server_on_receive_count;

            EXPECT_FALSE(error);

            std::shared_ptr<char> buf(new char[4], std::default_delete<char[]>());

            client.send_data(buf, 0); // silent fail

            client.send_data(buf, 0,
                [&](io::net::UdpPeer& client, const io::Error& error) {
                    ++server_on_send_count;
                    EXPECT_TRUE(error);
                    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
                    server->schedule_removal();
                }
            );
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::net::UdpClient(loop);
    client->set_destination({"127.0.0.1", m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            client.send_data("!",
                [&](io::net::UdpClient& client, const io::Error& error) {
                    ++client_on_send_count;
                    EXPECT_FALSE(error);
                });

            client.send_data("",
                [&](io::net::UdpClient& client, const io::Error& error) {
                    ++client_on_send_count;
                    EXPECT_TRUE(error);
                    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
                    client.schedule_removal();
                }
            );
        },
        [&](io::net::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            ++client_on_receive_count;
            EXPECT_FALSE(error);
        }
    );

    EXPECT_EQ(0, server_on_send_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, client_on_send_count);
    EXPECT_EQ(0, client_on_receive_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_send_count);
    EXPECT_EQ(1, server_on_receive_count);
    EXPECT_EQ(2, client_on_send_count);
    EXPECT_EQ(0, client_on_receive_count);
}

TEST_F(UdpClientServerTest, ipv6_address) {
    io::EventLoop loop;

    const std::string client_message = "client";
    const std::string server_message = "server";

    std::size_t client_on_receive_count = 0;
    std::size_t server_on_receive_count = 0;

    auto server = new io::net::UdpServer(loop);
    auto server_listen_error = server->start_receive({"::", m_default_port},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(server, &peer.server());

            ++server_on_receive_count;

            const std::string s(data.buf.get(), data.size);
            EXPECT_EQ(client_message, s);

            peer.send_data(server_message,
                [&](io::net::UdpPeer& peer, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string();
                    server->schedule_removal();
                }
            );
        }
    );
    EXPECT_FALSE(server_listen_error) << server_listen_error.string();

    auto client = new io::net::UdpClient(loop);
    client->set_destination({"::1", m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data(client_message,
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string();
                }
            );
        },
        [&](io::net::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_on_receive_count;

            const std::string s(data.buf.get(), data.size);
            EXPECT_EQ(server_message, s);
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(0, server_on_receive_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_receive_count);
    EXPECT_EQ(1, server_on_receive_count);
}

TEST_F(UdpClientServerTest, ipv6_peer_identity) {
    io::EventLoop loop;

    std::size_t server_on_timeout_count = 0;

    const io::net::UdpPeer* peer_1 = nullptr;
    const io::net::UdpPeer* peer_2 = nullptr;
    const io::net::UdpPeer* peer_3 = nullptr;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({"::", m_default_port},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            auto peer_id = std::stoi(std::string(data.buf.get(), data.size));
            switch (peer_id) {
                case 1: {
                    if (peer_1 == nullptr) {
                        peer_1 = &peer;
                        (new io::Timer(loop))->start(
                            {100, 100, 100, 100, 100},
                            [&](io::Timer& timer) {
                                if (timer.callback_call_counter() == 4) {
                                    peer.send_data("close");
                                    timer.schedule_removal();
                                } else {
                                    peer.send_data("!");
                                }
                            }
                        );
                    } else {
                        EXPECT_EQ(peer_1, &peer);
                    }
                    break;
                }

                case 2: {
                    if (peer_2 == nullptr) {
                        peer_2 = &peer;
                    } else {
                        EXPECT_EQ(peer_2, &peer);
                    }
                    break;
                }

                case 3: {
                    if (peer_3 == nullptr) {
                        peer_3 = &peer;
                    } else {
                        EXPECT_EQ(peer_3, &peer);
                    }
                    break;
                }

                default:
                    FAIL() << "Unexpected peer ID";
                    break;
            }
        },
        200,
        [&](io::net::UdpPeer& peer, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_timeout_count;
            if (server_on_timeout_count == 3) {
                server->schedule_removal();
            }
        }
    );

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto client_1 = new io::net::UdpClient(loop);
    client_1->set_destination({"::1", m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client.send_data("1");
        },
        [&](io::net::UdpClient& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();

            if (std::string(data.buf.get(), data.size) == "close") {
                client_1->send_data("1",
                    [](io::net::UdpClient& client, const io::Error& error) {
                        EXPECT_FALSE(error) << error.string();
                        client.schedule_removal();
                    }
                );
            }
        }
    );

    auto client_2 = new io::net::UdpClient(loop);
    client_2->set_destination({"::1", m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            (new io::Timer(loop))->start(
                {100, 100, 100, 100, 100},
                [&](io::Timer& timer) {
                    client_2->send_data("2");
                    if (timer.callback_call_counter() == 4) {
                        timer.schedule_removal();
                        client_2->schedule_removal();
                    }
                }
            );
        },
        [&](io::net::UdpClient& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        }
    );

    auto client_3 = new io::net::UdpClient(loop);
    client_3->set_destination({"::1", m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            client_3->send_data("3",
                [](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string();
                    client.schedule_removal();
                }
            );
        }
    );

    EXPECT_EQ(0, server_on_timeout_count);
    EXPECT_FALSE(peer_1);
    EXPECT_FALSE(peer_2);
    EXPECT_FALSE(peer_3);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(3, server_on_timeout_count);
    EXPECT_TRUE(peer_1);
    EXPECT_TRUE(peer_2);
    EXPECT_TRUE(peer_3);
}

TEST_F(UdpClientServerTest, client_works_with_multiple_servers) {
    std::size_t on_client_receive_from_server_1 = 0;
    std::size_t on_client_receive_from_server_2 = 0;
    std::size_t on_client_receive_from_server_3 = 0;

    io::EventLoop loop;

    auto server_1 = new io::net::UdpServer(loop);
    auto listen_error_1 = server_1->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            peer.send_data("1");
        }
    );
    ASSERT_FALSE(listen_error_1) << listen_error_1;

    auto server_2 = new io::net::UdpServer(loop);
    auto listen_error_2 = server_2->start_receive({m_default_addr, std::uint16_t(m_default_port + 1)},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            peer.send_data("2");
        }
    );
    ASSERT_FALSE(listen_error_2) << listen_error_2;

    auto server_3 = new io::net::UdpServer(loop);
    auto listen_error_3 = server_3->start_receive({m_default_addr, std::uint16_t(m_default_port + 2)},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            peer.send_data("3");
        }
    );
    ASSERT_FALSE(listen_error_3) << listen_error_3;

    const std::size_t CLIENT_TIMEOUT_MS = 1000;

    auto client_on_destination_set = [&](io::net::UdpClient& client, const io::Error& error) {
        EXPECT_FALSE(error) << error;
        client.send_data("!");
    };

    auto client_on_data_received = [&](io::net::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error) << error;
        const std::string s(data.buf.get(), data.size);
        if (s == "1") {
            ++on_client_receive_from_server_1;
            client.close();
        } else if (s == "2") {
            ++on_client_receive_from_server_2;
            client.close();
        } else if (s == "3") {
            ++on_client_receive_from_server_3;
        } else {
            FAIL() << "Unexpected data";
        }
    };

    std::function<void(io::net::UdpClient&, const io::Error&)> client_on_close;
    client_on_close = [&](io::net::UdpClient& client, const io::Error& error) {
        EXPECT_FALSE(error) << error;
        ASSERT_FALSE(client.is_open());

        std::uint16_t port = m_default_port;
        if (on_client_receive_from_server_1) {
            ++port;
        }

        if (on_client_receive_from_server_2) {
            ++port;
        }

        if (on_client_receive_from_server_1 &&
            on_client_receive_from_server_2 &&
            on_client_receive_from_server_3) {
            return;
        }

        client.set_destination({m_default_addr, port},
            client_on_destination_set,
            client_on_data_received,
            CLIENT_TIMEOUT_MS,
            client_on_close
        );
    };

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        client_on_destination_set,
        client_on_data_received,
        CLIENT_TIMEOUT_MS,
        client_on_close
    );

    (new io::Timer(loop))->start(
        500,
        [&](io::Timer& timer) {
            timer.schedule_removal();
            server_1->schedule_removal();
            server_2->schedule_removal();
            server_3->schedule_removal();
            client->schedule_removal();
        }
    );

    EXPECT_EQ(0, on_client_receive_from_server_1);
    EXPECT_EQ(0, on_client_receive_from_server_2);
    EXPECT_EQ(0, on_client_receive_from_server_3);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_client_receive_from_server_1);
    EXPECT_EQ(1, on_client_receive_from_server_2);
    EXPECT_EQ(1, on_client_receive_from_server_3);
}

TEST_F(UdpClientServerTest, client_set_destination_with_ipv4_address_then_with_ipv6) {
    io::EventLoop loop;

    std::size_t server_data_receive_count = 0;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({"::", std::uint16_t(m_default_port + 1)},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++server_data_receive_count;
            server->schedule_removal();
        }
    );
    ASSERT_FALSE(listen_error) << listen_error;

    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;

            client.close();
        },
        nullptr,
        1000,
        [&](io::net::UdpClient& client, const io::Error& error) {
            client.set_destination({"::1", std::uint16_t(m_default_port + 1)},
                [&](io::net::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error;

                    client.send_data("!",
                        [](io::net::UdpClient& client, const io::Error& error) {
                            EXPECT_FALSE(error) << error.string();
                            client.schedule_removal();
                        }
                    );
                }
            );
        }
    );

    EXPECT_EQ(0, server_data_receive_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_data_receive_count);
}

TEST_F(UdpClientServerTest, peers_count) {
    io::EventLoop loop;

    const std::size_t CLIENTS_COUNT = 5;

    std::set<long> client_ids;

    auto server = new io::net::UdpServer(loop);
    // TODO: "::" address works here. IPV6 and IPV4 are compatible on local host????
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            const auto client_index = std::stol(std::string(data.buf.get(), data.size));
            client_ids.insert(client_index);
            EXPECT_EQ(client_ids.size(), server->peers_count());
        },
        500,
        [&](io::net::UdpPeer& peer, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            if (!server->is_removal_scheduled()) {
                server->schedule_removal();
            }
        }
    );
    ASSERT_FALSE(listen_error) << listen_error;

    EXPECT_EQ(0, server->peers_count());

    for (std::size_t i = 0; i < CLIENTS_COUNT; ++i) {
        auto client = new io::net::UdpClient(loop);
        client->set_destination({m_default_addr, m_default_port},
            [&, i](io::net::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error) << error;

                client.send_data(std::to_string(i),
                    [&, i](io::net::UdpClient& client, const io::Error& error) {
                        EXPECT_FALSE(error) << error.string();

                        client.send_data(std::to_string(i),
                            [&](io::net::UdpClient& client, const io::Error& error) {
                                EXPECT_FALSE(error) << error.string();
                                client.schedule_removal();
                            }
                        );
                    }
                );
            }
        );
    }

    EXPECT_EQ(0, server->peers_count());

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(CLIENTS_COUNT, client_ids.size());
}

TEST_F(UdpClientServerTest, server_multiple_start_receive_in_row_different_addresses) {
    io::EventLoop loop;

    auto server = new io::net::UdpServer(loop);
    auto listen_error_1 = server->start_receive({m_default_addr, m_default_port},
        nullptr,
        500,
        nullptr
    );
    ASSERT_FALSE(listen_error_1) << listen_error_1;

    auto listen_error_2 = server->start_receive({"::", std::uint16_t(m_default_port + 1)},
        nullptr,
        500,
        nullptr
    );
    ASSERT_TRUE(listen_error_2);
    EXPECT_EQ(io::StatusCode::CONNECTION_ALREADY_IN_PROGRESS, listen_error_2.code());

    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, server_multiple_start_receive_sequenced_different_addresses) {
    io::EventLoop loop;

    auto server = new io::net::UdpServer(loop);
    auto listen_error_1 = server->start_receive({m_default_addr, m_default_port},
        nullptr,
        500,
        nullptr
    );
    ASSERT_FALSE(listen_error_1) << listen_error_1;

    server->close([&](io::net::UdpServer& server, const io::Error& error) {
        auto listen_error_2 = server.start_receive({"::", std::uint16_t(m_default_port + 1)},
            nullptr,
            500,
            nullptr
        );
        ASSERT_FALSE(listen_error_2) << listen_error_2;
        server.schedule_removal();
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, client_set_destination_and_simultaneously_send_1) {

    std::size_t on_send_callback_count = 0;

    io::EventLoop loop;
    // Test description: set destination and send data right away (not in callback). Should be error
    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            EXPECT_TRUE(client.is_open());
        }
    );
    EXPECT_FALSE(client->is_open());

    client->send_data("!!!",
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPERATION_CANCELED, error.code());
            ++on_send_callback_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, on_send_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_send_callback_count);
}

TEST_F(UdpClientServerTest, client_set_destination_and_simultaneously_send_2) {
    // The same test but using UDP with timeout execution path
    std::size_t on_send_callback_count = 0;

    io::EventLoop loop;
    // Test description: set destination and send data right away (not in callback). Should be error
    auto client = new io::net::UdpClient(loop);
    client->set_destination({m_default_addr, m_default_port},
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            EXPECT_TRUE(client.is_open());
        },
        nullptr,
        200,
        nullptr
    );
    EXPECT_FALSE(client->is_open());

    client->send_data("!!!",
        [&](io::net::UdpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPERATION_CANCELED, error.code());
            ++on_send_callback_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, on_send_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_send_callback_count);
}

TEST_F(UdpClientServerTest, server_0_ms_timeout_for_peer) {
    io::EventLoop loop;

    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        nullptr,
        0,
        nullptr
    );
    EXPECT_TRUE(listen_error);
    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, listen_error.code());
    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

// TOOD: test 0 expiration timeout for UDP client as invalid

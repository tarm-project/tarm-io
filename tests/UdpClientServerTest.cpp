/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/


#include "UTCommon.h"

#include "io/UdpClient.h"
#include "io/UdpServer.h"
#include "io/ScopeExitGuard.h"
#include "io/Timer.h"

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
    auto client = new io::UdpClient(loop);
    EXPECT_EQ(io::Endpoint::UNDEFINED, client->endpoint().type());

    client->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, server_default_state) {
    io::EventLoop loop;
    auto server = new io::UdpServer(loop);

    server->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, server_bind) {
    io::EventLoop loop;

    auto server = new io::UdpServer(loop);
    auto error = server->start_receive({m_default_addr, m_default_port}, nullptr);
    ASSERT_FALSE(error);
    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

#if defined(__APPLE__) || defined(__linux__)
// Windows does not have privileged ports
TEST_F(UdpClientServerTest, bind_privileged) {
    io::EventLoop loop;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(io::Error(io::StatusCode::PERMISSION_DENIED), server->start_receive({m_default_addr, 80}, nullptr));
    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}
#endif

TEST_F(UdpClientServerTest, server_address_in_use) {
    io::EventLoop loop;

    auto server_1 = new io::UdpServer(loop);
    auto listen_error_1 = server_1->start_receive({m_default_addr, m_default_port}, nullptr);
    EXPECT_FALSE(listen_error_1);

    auto server_2 = new io::UdpServer(loop);
    auto listen_error_2 = server_2->start_receive({m_default_addr, m_default_port}, nullptr);
    EXPECT_TRUE(listen_error_2);
    EXPECT_EQ(io::StatusCode::ADDRESS_ALREADY_IN_USE, listen_error_2.code());

    server_1->schedule_removal();
    server_2->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, server_invalid_address) {
    io::EventLoop loop;

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({"100500", m_default_port}, nullptr);
    EXPECT_TRUE(listen_error);
    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, listen_error.code());

    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, client_invalid_address) {
    io::EventLoop loop;

    auto client = new io::UdpClient(loop);
    auto error = client->set_destination({"bla", m_default_port});
    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());

    client->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, 1_client_send_data_to_server) {
    io::EventLoop loop;

    const std::string message = "Hello world!";

    std::size_t server_on_data_receive_count = 0;
    std::size_t client_on_data_send_count = 0;

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
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

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    EXPECT_EQ("127.0.0.1", client->endpoint().address_string());
    EXPECT_EQ(m_default_port, client->endpoint().port());
    client->send_data(message,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_data_send_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_on_data_receive_count);
    EXPECT_EQ(0, client_on_data_send_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_data_receive_count);
    EXPECT_EQ(1, client_on_data_send_count);
}

TEST_F(UdpClientServerTest, client_send_data_without_destination) {
    io::EventLoop loop;

    std::size_t on_send_call_count = 0;

    auto client = new io::UdpClient(loop);
    client->send_data("No way!",
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::DESTINATION_ADDRESS_REQUIRED, error.code());
            ++on_send_call_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, on_send_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_send_call_count);
}

TEST_F(UdpClientServerTest, client_get_buffer_size_1) {
    io::EventLoop loop;
    auto client = new io::UdpClient(loop);

    // At this point client has no receive callback nor destination set.
    // Even IP protocol version is undefined yet.
    const auto receive_buffer_1 = client->receive_buffer_size();
    EXPECT_TRUE(receive_buffer_1.error);

    const auto send_buffer_1 = client->send_buffer_size();
    EXPECT_TRUE(send_buffer_1.error);

    auto destination_error = client->set_destination({0x7F000001u, m_default_port});
    ASSERT_FALSE(destination_error);
    destination_error = client->set_destination({0x7F000001u, m_default_port});
    ASSERT_FALSE(destination_error);

    const auto receive_buffer_2 = client->receive_buffer_size();
    EXPECT_FALSE(receive_buffer_2.error);
    EXPECT_NE(0, receive_buffer_2.size);

    const auto send_buffer_2 = client->send_buffer_size();
    EXPECT_FALSE(send_buffer_2.error);
    EXPECT_NE(0, send_buffer_2.size);

    client->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, client_get_buffer_size_2) {
    io::EventLoop loop;
    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    auto receive_error = client->start_receive(
        [](io::UdpClient&, const io::DataChunk&, const io::Error&) {
        }
    );
    EXPECT_FALSE(receive_error);

    const auto receive_buffer_2 = client->receive_buffer_size();
    EXPECT_FALSE(receive_buffer_2.error);
    EXPECT_NE(0, receive_buffer_2.size);

    const auto send_buffer_2 = client->send_buffer_size();
    EXPECT_FALSE(send_buffer_2.error);
    EXPECT_NE(0, send_buffer_2.size);

    client->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, server_get_buffer_size_1) {
    io::EventLoop loop;

    auto server = new io::UdpServer(loop);
    const auto receive_buffer = server->receive_buffer_size();
    EXPECT_TRUE(receive_buffer.error);

    const auto send_buffer = server->send_buffer_size();
    EXPECT_TRUE(send_buffer.error);

    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, server_get_buffer_size_2) {
    io::EventLoop loop;

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::UdpPeer&, const io::DataChunk&, const io::Error&) {
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

TEST_F(UdpClientServerTest, client_set_buffer_size_1) {
    io::EventLoop loop;
    auto client = new io::UdpClient(loop);

    EXPECT_EQ(io::Error(io::StatusCode::ADDRESS_NOT_AVAILABLE), client->set_send_buffer_size(4096));
    EXPECT_EQ(io::Error(io::StatusCode::ADDRESS_NOT_AVAILABLE), client->set_receive_buffer_size(4096));

    client->set_destination({0x7F000001u, m_default_port});

    EXPECT_EQ(io::Error(io::StatusCode::OK), client->set_send_buffer_size(4096));
    EXPECT_EQ(io::Error(io::StatusCode::OK), client->set_receive_buffer_size(4096));

    auto receive_buffer = client->receive_buffer_size();
    EXPECT_FALSE(receive_buffer.error);
    EXPECT_EQ(4096, receive_buffer.size);

    auto send_buffer = client->send_buffer_size();
    EXPECT_FALSE(send_buffer.error);
    EXPECT_EQ(4096, send_buffer.size);

    EXPECT_EQ(io::Error(io::StatusCode::INVALID_ARGUMENT), client->set_send_buffer_size(0));
    EXPECT_EQ(io::Error(io::StatusCode::INVALID_ARGUMENT), client->set_receive_buffer_size(0));

    receive_buffer = client->receive_buffer_size();
    EXPECT_FALSE(receive_buffer.error);
    EXPECT_EQ(4096, receive_buffer.size);

    send_buffer = client->send_buffer_size();
    EXPECT_FALSE(send_buffer.error);
    EXPECT_EQ(4096, send_buffer.size);

    client->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, client_set_buffer_size_2) {
    io::EventLoop loop;
    auto client = new io::UdpClient(loop);

    EXPECT_EQ(io::Error(io::StatusCode::ADDRESS_NOT_AVAILABLE), client->set_send_buffer_size(4096));
    EXPECT_EQ(io::Error(io::StatusCode::ADDRESS_NOT_AVAILABLE), client->set_receive_buffer_size(4096));

    client->start_receive([](io::UdpClient&, const io::DataChunk&, const io::Error&) {
    });

    EXPECT_EQ(io::Error(io::StatusCode::OK), client->set_send_buffer_size(4096));
    EXPECT_EQ(io::Error(io::StatusCode::OK), client->set_receive_buffer_size(4096));

    auto receive_buffer = client->receive_buffer_size();
    EXPECT_FALSE(receive_buffer.error);
    EXPECT_EQ(4096, receive_buffer.size);

    auto send_buffer = client->send_buffer_size();
    EXPECT_FALSE(send_buffer.error);
    EXPECT_EQ(4096, send_buffer.size);

    EXPECT_EQ(io::Error(io::StatusCode::INVALID_ARGUMENT), client->set_send_buffer_size(4000000000u));
    EXPECT_EQ(io::Error(io::StatusCode::INVALID_ARGUMENT), client->set_receive_buffer_size(4000000000u));

    receive_buffer = client->receive_buffer_size();
    EXPECT_FALSE(receive_buffer.error);
    EXPECT_EQ(4096, receive_buffer.size);

    send_buffer = client->send_buffer_size();
    EXPECT_FALSE(send_buffer.error);
    EXPECT_EQ(4096, send_buffer.size);

    client->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(UdpClientServerTest, server_set_buffer_size) {
    io::EventLoop loop;

    auto server = new io::UdpServer(loop);

    EXPECT_EQ(io::Error(io::StatusCode::ADDRESS_NOT_AVAILABLE), server->set_send_buffer_size(4096));
    EXPECT_EQ(io::Error(io::StatusCode::ADDRESS_NOT_AVAILABLE), server->set_receive_buffer_size(4096));

    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::UdpPeer&, const io::DataChunk&, const io::Error&) {
        }
    );

    EXPECT_FALSE(listen_error);

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
#ifdef __APPLE__
    // At least on MAC OS X receive buffer should be larger (N + 16) than send buffer
    // to be able to receive packets of size N
    + 16
#endif
    ;

    std::size_t server_on_send_counter = 0;
    std::size_t server_on_receive_counter = 0;
    std::size_t client_on_send_counter = 0;
    std::size_t client_on_receive_counter = 0;

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_counter;

            peer.send_data("!",
                [&](io::UdpPeer& peer, const io::Error& error) {
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

    auto client = new io::UdpClient(loop);
        EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    auto client_receive_start_error = client->start_receive(
        [&](io::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_receive_counter;
            client.schedule_removal();
        }
    );
    ASSERT_FALSE(client_receive_start_error);
    EXPECT_FALSE(client->set_send_buffer_size(min_send_buffer_size));
    EXPECT_FALSE(client->set_receive_buffer_size(min_receive_buffer_size + 16));

    std::shared_ptr<char> buf(new char[min_send_buffer_size], std::default_delete<char[]>());
    std::memset(buf.get(), 0, min_send_buffer_size);
    client->send_data(buf, min_send_buffer_size,
        [&](io::UdpClient& peer, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_on_send_counter;
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

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);

        if (server_receive_counter == 0) {
            peer.set_user_data(&server_receive_counter);
        } else {
            // This is new instance of io::UdpPeer
            EXPECT_EQ(nullptr, peer.user_data());
        }

        ++server_receive_counter;

        std::string s(data.buf.get(), data.size);
        if (s == client_message_2) {
            server->schedule_removal();
        }
    }); // note: callback without timeout set

    EXPECT_FALSE(listen_error);

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    client->send_data(client_message_1,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data(client_message_2,
                [&](io::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    client.schedule_removal();
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

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
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

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    client->send_data(client_message_1,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data(client_message_2,
                [&](io::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    client.schedule_removal();
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

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::UdpPeer& peer, const io::Error& error) {
            EXPECT_FALSE(error);

            if (!on_new_peer_counter) {
                EXPECT_EQ(0, on_data_receive_counter);
            }

            ++on_new_peer_counter;
        },
        [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++on_data_receive_counter;

            if (on_data_receive_counter == 4) {
                server->schedule_removal();
            }
        },
    100,
    nullptr);

    auto client_1 = new io::UdpClient(loop);
    EXPECT_FALSE(client_1->set_destination({0x7F000001u, m_default_port}));
    client_1->send_data("1_1",
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data("1_2",
                [&](io::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    client.schedule_removal();
                }
            );
        }
    );

    auto client_2 = new io::UdpClient(loop);
    EXPECT_FALSE(client_2->set_destination({0x7F000001u, m_default_port}));
    client_2->send_data("2_1",
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data("2_2",
                [&](io::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    client.schedule_removal();
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

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
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
    [&](io::UdpPeer& client, const io::Error& error) {
        EXPECT_FALSE(error);

        ASSERT_TRUE(client.user_data());
        auto& value = *reinterpret_cast<std::size_t*>(client.user_data());
        EXPECT_EQ(2, value);

        ++server_timeout_counter;
    });

    EXPECT_FALSE(listen_error);

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    client->send_data(client_message_1,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_send_counter;
        }
    );

    auto timer_1 = new io::Timer(loop);
    timer_1->start(100, [&](io::Timer& timer) {
        EXPECT_EQ(1, client_send_counter);
        client->send_data(client_message_2,
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                ++client_send_counter;
            }
        );
        timer.schedule_removal();
    });

    auto timer_2 = new io::Timer(loop);
    timer_2->start(600, [&](io::Timer& timer) {
        EXPECT_EQ(2, client_send_counter);
        client->send_data(client_message_3,
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
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

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::UdpPeer& client, const io::DataChunk& data, const io::Error& error) {
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
    [&](io::UdpPeer& client, const io::Error& error) {
        EXPECT_FALSE(error);
        ASSERT_TRUE(client.user_data());

        const auto& data_str = *reinterpret_cast<std::string*>(client.user_data());
        peer_to_close_count[data_str]++;
    });

    auto client_1 = new io::UdpClient(loop);
    EXPECT_FALSE(client_1->set_destination({0x7F000001u, m_default_port}));
    auto timer_1 = new io::Timer(loop);
    timer_1->start(0, 200, [&](io::Timer& timer) {
        client_1->send_data(client_1_message,
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
            }
        );
    });

    EXPECT_FALSE(listen_error);

    auto client_2 = new io::UdpClient(loop);
    EXPECT_FALSE(client_2->set_destination({0x7F000001u, m_default_port}));
    auto timer_2 = new io::Timer(loop);
    timer_2->start(0, 400, [&](io::Timer& timer) {
        client_2->send_data(client_2_message,
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
            }
        );
    });

    auto client_3 = new io::UdpClient(loop);
    EXPECT_FALSE(client_3->set_destination({0x7F000001u, m_default_port}));
    auto timer_3 = new io::Timer(loop);
    timer_3->start(0, 800, [&](io::Timer& timer) {
        client_3->send_data(client_3_message,
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
            }
        );
    });

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

/*
TEST_F(UdpClientServerTest, multiple_clients_timeout_for_server) {
    // Note: timings are essential in this test
    // Timeout : 210ms
    // UdpClient1 send:    0 - 100 - 200 - 300 - 400 - 500 - 600 | - - - -x- - - - -
    // UdpClient2 send:    0 - - - - 200 - - - - 400 - - - - 600 | - - - -x- - - - -
    // UdpClient3 send:    0 - - - - - x - - - - 400 - - - - - x | - - - - - - - - -
    //                                                           | <- stop sending here
    //                                                                  terminate there -> ...
    // 'x' - is client timeout

    io::EventLoop loop;

    std::string client_1_message = "client_1";
    std::string client_2_message = "client_2";
    std::string client_3_message = "client_3";

    std::unordered_map<std::string, std::size_t> peer_to_close_count;

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::UdpPeer& client, const io::DataChunk& data, const io::Error& error) {
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
    220, // timeout MS
    [&](io::UdpPeer& client, const io::Error& error) {
        EXPECT_FALSE(error);
        ASSERT_TRUE(client.user_data());

        const auto& data_str = *reinterpret_cast<std::string*>(client.user_data());
        peer_to_close_count[data_str]++;
    });

    auto client_1 = new io::UdpClient(loop);
    EXPECT_FALSE(client_1->set_destination({0x7F000001u, m_default_port}));
    auto timer_1 = new io::Timer(loop);
    timer_1->start(0, 100, [&](io::Timer& timer) {
        client_1->send_data(client_1_message,
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
            }
        );
    });

    EXPECT_FALSE(listen_error);

    auto client_2 = new io::UdpClient(loop);
    EXPECT_FALSE(client_2->set_destination({0x7F000001u, m_default_port}));
    auto timer_2 = new io::Timer(loop);
    timer_2->start(0, 200, [&](io::Timer& timer) {
        client_2->send_data(client_2_message,
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
            }
        );
    });

    auto client_3 = new io::UdpClient(loop);
    EXPECT_FALSE(client_3->set_destination({0x7F000001u, m_default_port}));
    auto timer_3 = new io::Timer(loop);
    timer_3->start(0, 400, [&](io::Timer& timer) {
        client_3->send_data(client_3_message,
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
            }
        );
    });

    auto timer_stop_all = new io::Timer(loop);
    timer_stop_all->start(650, [&](io::Timer& timer) {
        timer_1->stop();
        timer_2->stop();
        timer_3->stop();
        timer.schedule_removal();
    });

    auto timer_remove = new io::Timer(loop);
    timer_remove->start(1000, [&](io::Timer& timer) {
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
*/

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

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::UdpPeer& client, const io::DataChunk& data, const io::Error& error) {
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
        [&](io::UdpPeer& client, const io::Error& error) {
            ++peer_timeout_counter;
            EXPECT_FALSE(error);
            t2 = std::chrono::high_resolution_clock::now();

            EXPECT_TIMEOUT_MS(EXPECTED_ELAPSED_TIME_MS,
                              std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1));

            server->schedule_removal();
        }
    );

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    auto receive_error = client->start_receive(
        [&](io::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            ++client_data_receive_counter;

            if (client_data_receive_counter == send_timeouts.size()) {
                client.schedule_removal();
            }
        }
    );
    EXPECT_FALSE(receive_error);

    client->send_data("!!!");

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

    auto server = new io::UdpServer(loop);
    server->close(
        [&](io::UdpServer& server, const io::Error& error) {
            ++server_close_1_callback_call_count; // Should not be called
        }
    );

    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::UdpPeer& peer, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        10000,
        [&](io::UdpPeer& peer, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_peer_timeout_call_count;
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({m_default_addr, m_default_port}));
    client->send_data("!!!",
        [](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            client.schedule_removal();
        }
    );

    server->close(
        [&](io::UdpServer& server, const io::Error& error) {
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

TEST_F(UdpClientServerTest, server_schedule_removal_in_close_callback) {
    io::EventLoop loop;

    std::size_t server_close_callback_call_count = 0;

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        }
    );
    ASSERT_FALSE(listen_error);

    server->close(
        [&](io::UdpServer& server, const io::Error& error) {
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

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        ++server_data_receive_counter;

        std::string s(data.buf.get(), data.size);
        EXPECT_EQ(client_message, s);

        peer.send_data(server_message,
            [&](io::UdpPeer& peer, const io::Error& error) {
                EXPECT_FALSE(error) << error.string();
                ++server_data_send_counter;
                server->schedule_removal();
            }
        );
    });
    ASSERT_FALSE(listen_error);

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    auto receive_error = client->start_receive(
        [&](io::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            std::string s(data.buf.get(), data.size);
            EXPECT_EQ(server_message, s);

            ++client_data_receive_counter;
            client.schedule_removal();
        }
    );
    EXPECT_FALSE(receive_error);

    client->send_data(client_message,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_data_send_counter;
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

    auto server = new io::UdpServer(loop);

    std::function<void(io::UdpPeer&, const io::Error&)> on_server_send =
        [&](io::UdpPeer& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_send_counter;

            if (server_data_send_counter < 2) {
                client.send_data(server_message[server_data_send_counter], on_server_send);
            } else {
                server->schedule_removal();
            }
        };

    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);

        ++server_data_receive_counter;
        peer.send_data(server_message[server_data_send_counter], on_server_send);
    });

    EXPECT_FALSE(listen_error);

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    auto receive_error = client->start_receive(
        [&](io::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            std::string s(data.buf.get(), data.size);
            EXPECT_EQ(server_message[client_data_receive_counter], s);

            ++client_data_receive_counter;

            if (client_data_receive_counter == 2) {
                client.schedule_removal();
            }
        }
    );
    EXPECT_FALSE(receive_error);

    client->send_data(client_message,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_data_send_counter;
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

    auto client_2 = new io::UdpClient(loop);

    auto client_1 = new io::UdpClient(loop);
    auto receive_error = client_1->start_receive(
        [&](io::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++receive_callback_call_count;
        }
    );
    EXPECT_FALSE(receive_error);

    client_1->set_destination({0x7F000001u, m_default_port});
    client_1->send_data(client_message,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            // Note: Client1 bound port is only known when it make some activity like sending data
            client_2->set_destination({0x7F000001u, client.bound_port()});
            client_2->send_data(other_message,
                [&](io::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                }
            );
        }
    );

    auto timer = new io::Timer(loop);
    timer->start(100,
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

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
    [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
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

    auto client = new io::UdpClient(loop);
    client->set_destination({0x7F000001u, m_default_port});
    client->send_data(message, SIZE,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            data_sent = true;
            client.schedule_removal();
        }
    );

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_TRUE(data_sent);
    EXPECT_TRUE(data_received);
}

TEST_F(UdpClientServerTest, send_larger_than_allowed_to_send) {
    io::EventLoop loop;

    std::size_t SIZE = 100 * 1024;

    std::shared_ptr<char> message(new char[SIZE], std::default_delete<char[]>());
    std::memset(message.get(), 0, SIZE);

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    client->send_data(message, SIZE,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::MESSAGE_TOO_LONG, error.code());
            client.schedule_removal();
        }
    );

    ASSERT_EQ(io::StatusCode::OK, loop.run());
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

    std::function<void(io::UdpPeer&, const io::Error&)> server_send =
        [&](io::UdpPeer& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_send_message_counter;
            if (server_send_message_counter < SIZE) {
                client.send_data(message, SIZE - server_send_message_counter, server_send);
            }
        };

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::UdpPeer& client, const io::DataChunk& chunk, const io::Error& error) {
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

    std::function<void(io::UdpClient&, const io::Error&)> client_send =
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_send_message_counter;
            if (client_send_message_counter < SIZE) {
                client.send_data(message, SIZE - client_send_message_counter, client_send);
            }
        };

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    auto receive_error = client->start_receive(
        [&](io::UdpClient&, const io::DataChunk& chunk, const io::Error& error) {
            EXPECT_FALSE(error);
            ASSERT_EQ(SIZE - client_receive_message_counter, chunk.size);
            for (std::size_t i = 0; i < SIZE - client_receive_message_counter; ++i) {
                ASSERT_EQ(message.get()[i], chunk.buf.get()[i]) << "i= " << i;
            }

            ++client_receive_message_counter;
        }
    );
    EXPECT_FALSE(receive_error);

    client->send_data(message, SIZE - client_send_message_counter, client_send);

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
/*
 ENOBUFS
 The output queue for a network interface was full.  This generally indicates that the interface has stopped sending, but may  be
 caused by transient congestion.  (Normally, this does not occur in Linux.  Packets are just silently dropped when a device queue
 overflows.)
 */

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
        std::function<void(io::UdpPeer&, const io::Error&)> on_server_send =
            [&](io::UdpPeer& client, const io::Error& error) {
                EXPECT_FALSE(error);

                ++server_send_message_counter;
                if (server_send_message_counter < SIZE) {
                    client.send_data(message, SIZE - server_send_message_counter, on_server_send);
                }
            };

        io::EventLoop server_loop;

        auto server = new io::UdpServer(server_loop);
        auto listen_error = server->start_receive({m_default_addr, m_default_port},
            [&](io::UdpPeer& client, const io::DataChunk& chunk, const io::Error& error) {
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
        std::function<void(io::UdpClient&, const io::Error&)> client_send =
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);

                ++client_send_message_counter;
                if (client_send_message_counter < SIZE) {
                    client.send_data(message, SIZE - client_send_message_counter, client_send);
                }
            };

        io::EventLoop client_loop;

        auto client = new io::UdpClient(client_loop);
        EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
        auto receive_error = client->start_receive(
            [&](io::UdpClient& client, const io::DataChunk& chunk, const io::Error& error) {
                EXPECT_FALSE(error);
                for (std::size_t i = 0; i < chunk.size; ++i) {
                    ASSERT_EQ(message.get()[i], chunk.buf.get()[i]) << "i= " << i;
                }

                ++client_receive_message_counter;
            }
        );
        EXPECT_FALSE(receive_error);

        client->send_data(message, SIZE, client_send);

        auto timer = new io::Timer(client_loop);
        timer->start(100, 100,
            [&](io::Timer& timer) {
                //std::cout << "c " << client_receive_message_counter << " " << client_send_message_counter << std::endl;
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

    auto server = new io::UdpServer(loop);
    io::Error listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_receive_counter;
        }
    );
    EXPECT_FALSE(listen_error);

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    client->schedule_removal();
    client->send_data("Hello",
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPERATION_CANCELED, error.code());
            ++client_send_counter;
            server->schedule_removal();
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

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    auto receive_error = client->start_receive(
        [&](io::UdpClient& client, const io::DataChunk& chunk, const io::Error& error) {
        },
        TIMEOUT,
        [&](io::UdpClient& client, const io::Error& error) {
            client.schedule_removal();
            t2 = std::chrono::high_resolution_clock::now();
        }
    );
    EXPECT_FALSE(receive_error);

    EXPECT_NE(0, client->bound_port());

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_TIMEOUT_MS(TIMEOUT, std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1));
}

TEST_F(UdpClientServerTest, client_with_timeout_2) {
    // Note: testing that after timeout data is not sent anymore
    io::EventLoop loop;

    std::size_t server_receive_counter = 0;
    std::size_t client_send_counter = 0;

    auto server = new io::UdpServer(loop);
    io::Error listen_error = server->start_receive({m_default_addr, m_default_port},
        [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_receive_counter;
        }
    );
    EXPECT_FALSE(listen_error);

    const std::size_t TIMEOUT = 200;

    const auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    auto receive_error = client->start_receive(
        [&](io::UdpClient& client, const io::DataChunk& chunk, const io::Error& error) {
        },
        TIMEOUT,
        [&](io::UdpClient& client, const io::Error& error) {
            t2 = std::chrono::high_resolution_clock::now();
            EXPECT_TIMEOUT_MS(TIMEOUT, std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
            client.send_data("!!!",
                [&](io::UdpClient& client, const io::Error& error) {
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
    EXPECT_FALSE(receive_error);

    EXPECT_NE(0, client->bound_port());
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

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    auto receive_error = client->start_receive(
        nullptr,
        CLIENT_TIMEOUT,
        [&](io::UdpClient& client, const io::Error& error) {
            ++client_on_timeout_count;

            t2 = std::chrono::high_resolution_clock::now();
            EXPECT_TIMEOUT_MS(EXPECTED_ELAPSED_TIME,
                              std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() - send_timers_overdue.count());

            client.schedule_removal();
        }
    );
    EXPECT_FALSE(receive_error);

    auto timer = new io::Timer(loop);
    timer->start(
        send_timeouts,
        [&](io::Timer& timer) {
            send_timers_overdue += timer.real_time_passed_since_last_callback() - std::chrono::milliseconds(timer.timeout_ms());
            ASSERT_EQ(0, client_on_timeout_count);
            client->send_data("!!!",
            [&](io::UdpClient& client, const io::Error& error) {
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

    auto server = new io::UdpServer(loop);
    auto error = server->start_receive(
        { m_default_addr, m_default_port } ,
        [&] (io::UdpPeer&, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_new_peer_callback_count;
        },
        [&] (io::UdpPeer& peer, const io::DataChunk&, const io::Error& error) {
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
        [&] (io::UdpPeer&, const io::Error&){
            ++server_on_peer_timeout_callback_count;
        }
    );
    EXPECT_FALSE(error);

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));

    auto cleint_send_timer = new io::Timer(loop);
    cleint_send_timer->start(
        1,
        1,
        [&](io::Timer& timer) {
            client->send_data("!");
            if (server_on_data_receive_callback_count == 2) {
                server->schedule_removal();
                timer.schedule_removal();
                client->schedule_removal();
            }
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

    auto server = new io::UdpServer(loop);
    auto error = server->start_receive(
        { m_default_addr, m_default_port } ,
        [&] (io::UdpPeer&, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_new_peer_callback_count;
        },
        [&] (io::UdpPeer& peer, const io::DataChunk&, const io::Error&) {
            peer.close(INACTIVE_TIMEOUT);

            ++server_on_data_receive_callback_count;
        },
        CLIENT_TIMEOUT,
        [&] (io::UdpPeer&, const io::Error&){
            ++server_on_peer_timeout_callback_count;
        }
    );
    EXPECT_FALSE(error);

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    client->send_data("!");

    auto cleint_send_timer = new io::Timer(loop);
    cleint_send_timer->start(
        INACTIVE_TIMEOUT * 2,
        [&](io::Timer& timer) {
            server->schedule_removal();
            timer.schedule_removal();
            client->schedule_removal();
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

    auto server = new io::UdpServer(loop);
    auto error = server->start_receive(
        { m_default_addr, m_default_port } ,
        [&] (io::UdpPeer& peer, const io::DataChunk&, const io::Error&) {
            peer.close(INACTIVE_TIMEOUT_1);
            peer.close(INACTIVE_TIMEOUT_2); // This will be ignored

            ++server_on_data_receive_callback_count;
        },
        1000,
        nullptr
    );
    EXPECT_FALSE(error);

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    client->send_data("!");

    auto cleint_send_timer = new io::Timer(loop);
    cleint_send_timer->start(
        INACTIVE_TIMEOUT_2 * 2,
        [&] (io::Timer& timer) {
            client->send_data("!");
            timer.schedule_removal();
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

    auto server = new io::UdpServer(loop);
    auto error = server->start_receive(
        { m_default_addr, m_default_port } ,
        [&] (io::UdpPeer& peer, const io::DataChunk&, const io::Error&) {
            peer.close(INACTIVE_TIMEOUT);

            ++server_on_data_receive_callback_count;

            peer.send_data("!",
                [&](io::UdpPeer& peer, const io::Error& error) {
                    EXPECT_TRUE(error);
                    ++peer_on_send_callback_count;
                }
            );
        },
        1000 * 5,
        nullptr
    );
    EXPECT_FALSE(error);

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({0x7F000001u, m_default_port}));
    client->send_data("!");

    auto cleint_send_timer = new io::Timer(loop);
    cleint_send_timer->start(
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

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({"0.0.0.0", m_default_port},
        [&](io::UdpPeer& client, const io::DataChunk& data, const io::Error& error) {
            ++server_on_receive_count;

            EXPECT_FALSE(error);

            std::shared_ptr<char> buf(new char[4], std::default_delete<char[]>());

            client.send_data(buf, 0); // silent fail

            client.send_data(buf, 0,
                [&](io::UdpPeer& client, const io::Error& error) {
                    ++server_on_send_count;
                    EXPECT_TRUE(error);
                    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
                    server->schedule_removal();
                }
            );
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({"127.0.0.1", m_default_port}));
    auto receive_error = client->start_receive(
        [&](io::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            ++client_on_receive_count;
            EXPECT_FALSE(error);
        }
    );
    EXPECT_FALSE(receive_error);

    client->send_data("!",
        [&](io::UdpClient& client, const io::Error& error) {
            ++client_on_send_count;
            EXPECT_FALSE(error);
        });

    client->send_data("",
        [&](io::UdpClient& client, const io::Error& error) {
            ++client_on_send_count;
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
            client.schedule_removal();
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

    auto server = new io::UdpServer(loop);
    auto server_listen_error = server->start_receive({"::", m_default_port},
        [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(server, &peer.server());

            ++server_on_receive_count;

            const std::string s(data.buf.get(), data.size);
            EXPECT_EQ(client_message, s);

            peer.send_data(server_message,
                [&](io::UdpPeer& peer, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string();
                    server->schedule_removal();
                }
            );
        }
    );
    EXPECT_FALSE(server_listen_error) << server_listen_error.string();

    auto client = new io::UdpClient(loop);
    EXPECT_FALSE(client->set_destination({"::1", m_default_port}));
    client->send_data(client_message,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        }
    );

    auto client_listen_error = client->start_receive(
        [&](io::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_on_receive_count;

            const std::string s(data.buf.get(), data.size);
            EXPECT_EQ(server_message, s);
            client.schedule_removal();
        }
    );
    EXPECT_FALSE(client_listen_error) << client_listen_error.string();

    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(0, server_on_receive_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_receive_count);
    EXPECT_EQ(1, server_on_receive_count);
}

TEST_F(UdpClientServerTest, ipv6_peer_identity) {
    io::EventLoop loop;

    std::size_t server_on_timeout_count = 0;

    const io::UdpPeer* peer_1 = nullptr;
    const io::UdpPeer* peer_2 = nullptr;
    const io::UdpPeer* peer_3 = nullptr;

    auto server = new io::UdpServer(loop);
    auto listen_error = server->start_receive({"::", m_default_port},
        [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
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
        [&](io::UdpPeer& peer, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_timeout_count;
            if (server_on_timeout_count == 3) {
                server->schedule_removal();
            }
        }
    );

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto client_1 = new io::UdpClient(loop);
    client_1->set_destination({"::1", m_default_port});
    client_1->start_receive(
        [&](io::UdpClient& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();

            if (std::string(data.buf.get(), data.size) == "close") {
                client_1->send_data("1",
                    [](io::UdpClient& client, const io::Error& error) {
                        EXPECT_FALSE(error) << error.string();
                        client.schedule_removal();
                    }
                );
            }
        }
    );
    client_1->send_data("1");

    auto client_2 = new io::UdpClient(loop);
    client_2->set_destination({"::1", m_default_port});
    client_2->start_receive(
        [&](io::UdpClient& peer, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        }
    );
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

    auto client_3 = new io::UdpClient(loop);
    client_3->set_destination({"::1", m_default_port});
    client_3->send_data("3",
        [](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            client.schedule_removal();
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

// TODO: check address of UDP peer

// TODO: set_destination with ipv4 address athan with ipv6
// TODO: null send buf

// TODO: client start receive without destination set???? Allow receive from any peer????

// TODO: server receive close and receive again

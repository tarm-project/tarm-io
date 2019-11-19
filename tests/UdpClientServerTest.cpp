
#include "UTCommon.h"

#include "io/UdpClient.h"
#include "io/UdpServer.h"
#include "io/ScopeExitGuard.h"
#include "io/Timer.h"

#include <cstdint>
#include <unordered_map>

struct UdpClientServerTest : public testing::Test,
                             public LogRedirector {

protected:
    std::uint16_t m_default_port = 31541;
    std::string m_default_addr = "127.0.0.1";
};

TEST_F(UdpClientServerTest, client_default_constructor) {
    io::EventLoop loop;
    auto client = new io::UdpClient(loop);

    client->schedule_removal();
    ASSERT_EQ(0, loop.run());
}

TEST_F(UdpClientServerTest, server_default_constructor) {
    io::EventLoop loop;
    auto server = new io::UdpServer(loop);

    server->schedule_removal();
    ASSERT_EQ(0, loop.run());
}

TEST_F(UdpClientServerTest, server_bind) {
    io::EventLoop loop;

    auto server = new io::UdpServer(loop);
    auto error = server->bind(m_default_addr, m_default_port);
    ASSERT_FALSE(error);
    server->schedule_removal();

    ASSERT_EQ(0, loop.run());
}

#if defined(__APPLE__) || defined(__linux__)
// Windows does not have privileged ports
TEST_F(UdpClientServerTest, bind_privileged) {
    io::EventLoop loop;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(io::Error(io::StatusCode::PERMISSION_DENIED), server->bind(m_default_addr, 80));
    server->schedule_removal();

    ASSERT_EQ(0, loop.run());
}
#endif

TEST_F(UdpClientServerTest, 1_client_send_data_to_server) {
    io::EventLoop loop;

    const std::string message = "Hello world!";

    bool data_sent = false;
    bool data_received = false;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(io::Error(0), server->bind(m_default_addr, m_default_port));
    server->start_receive([&](io::UdpServer& server, io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        data_received = true;
        std::string s(data.buf.get(), data.size);
        EXPECT_EQ(message, s);
        server.schedule_removal();
    });

    auto client = new io::UdpClient(loop, 0x7F000001, m_default_port);
    client->send_data(message,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            data_sent = true;
            client.schedule_removal();
        });

    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(data_sent);
    EXPECT_TRUE(data_received);
}

TEST_F(UdpClientServerTest, peer_identity_without_preservation_on_server) {
    io::EventLoop loop;

    const std::string client_message_1 = "message_1";
    const std::string client_message_2 = "close";

    std::size_t server_receive_counter = 0;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(io::Error(0), server->bind(m_default_addr, m_default_port));
    server->start_receive([&](io::UdpServer& server, io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
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
            server.schedule_removal();
        }
    }); // note: callback without timeout set

    // TODO: replace all 0x7F000001 with test-defined constant
    auto client = new io::UdpClient(loop, 0x7F000001, m_default_port);
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

    ASSERT_EQ(0, loop.run());

    ASSERT_EQ(2, server_receive_counter);
}

TEST_F(UdpClientServerTest, peer_identity_with_preservation_on_server) {
    io::EventLoop loop;

    const std::string client_message_1 = "message_1";
    const std::string client_message_2 = "close";

    std::size_t server_receive_counter = 0;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(io::Error(0), server->bind(m_default_addr, m_default_port));
    server->start_receive([&](io::UdpServer& server, io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);

        if (server_receive_counter == 0) {
            peer.set_user_data(&server_receive_counter);
        } else {
            EXPECT_EQ(&server_receive_counter, peer.user_data());
        }

        ++server_receive_counter;

        std::string s(data.buf.get(), data.size);
        if (s == client_message_2) {
            server.schedule_removal();
        }
    },
    1000,
    nullptr);

    auto client = new io::UdpClient(loop, 0x7F000001, m_default_port);
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

    ASSERT_EQ(0, loop.run());

    ASSERT_EQ(2, server_receive_counter);
}

TEST_F(UdpClientServerTest, client_timeout_for_server) {
    // Note: timings are essential in this test
    // UdpServer timeouts: 0 - - - - 200 - - - - 400 - - - - 600 - - - -
    // UdpClient sends:    0 - 100 - - - - - - - - - - - - - 600
    //                          |-----| <- interval less than timeout. Decision: keep
    //                          |-----------------| <- interval more than timeout. Decision: drop
    //                                                      | <- register previos client as new one at this point

    io::EventLoop loop;

    const std::string client_message_1 = "message_1";
    const std::string client_message_2 = "message_2";
    const std::string client_message_3 = "message_3";

    std::size_t server_receive_counter = 0;
    std::size_t server_timeout_counter = 0;
    std::size_t client_send_counter = 0;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(io::Error(0), server->bind(m_default_addr, m_default_port));
    server->start_receive([&](io::UdpServer& server, io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
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
            server.schedule_removal();
        }
    },
    200, // timeout MS
    [&](io::UdpServer& server, io::UdpPeer& client) {
        ASSERT_TRUE(client.user_data());
        auto& value = *reinterpret_cast<decltype(server_receive_counter)*>(client.user_data());
        EXPECT_EQ(2, value);
        ++server_timeout_counter;
    });

    auto client = new io::UdpClient(loop, 0x7F000001, m_default_port);
    client->send_data(client_message_1,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_send_counter;
        }
    );

    io::Timer timer_1(loop);
    timer_1.start(100, [&](io::Timer& timer) {
        EXPECT_EQ(1, client_send_counter);
        client->send_data(client_message_2,
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                ++client_send_counter;
            }
        );
    });

    io::Timer timer_2(loop);
    timer_2.start(600, [&](io::Timer& timer) {
        EXPECT_EQ(2, client_send_counter);
        client->send_data(client_message_3,
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                ++client_send_counter;
                client.schedule_removal();
            }
        );
    });

    EXPECT_EQ(0, client_send_counter);
    EXPECT_EQ(0, server_receive_counter);
    EXPECT_EQ(0, server_timeout_counter);

    ASSERT_EQ(0, loop.run());

    ASSERT_EQ(3, client_send_counter);
    ASSERT_EQ(3, server_receive_counter);
    EXPECT_EQ(1, server_timeout_counter);
}

TEST_F(UdpClientServerTest, multiple_clients_timeout_for_server) {
    // Note: timings are essential in this test
    // UdpServer timeouts: 0 - - - - 200 - - - - 400 - - - - 600 | - - - 800 - - - - 1000
    // UdpClient1 send:    0 - 100 - 200 - 300 - 400 - 500 - 600 | - - - -x- - - - -
    // UdpClient2 send:    0 - - - - 200 - - - - 400 - - - - 600 | - - - -x- - - - -
    // UdpClient3 send:    0 - - - - -x- - - - - 400 - - - - -x- | - - - - - - - - -
    //                                                           | <- stop sending here
    //                                                                  terminate there -> ...
    // 'x' - is client timeout

    io::EventLoop loop;

    std::string client_1_message = "client_1";
    std::string client_2_message = "client_2";
    std::string client_3_message = "client_3";

    std::unordered_map<std::string, std::size_t> peer_to_close_count;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(io::Error(0), server->bind(m_default_addr, m_default_port));
    server->start_receive([&](io::UdpServer& server, io::UdpPeer& client, const io::DataChunk& data, const io::Error& error) {
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
    200, // timeout MS
    [&](io::UdpServer& server, io::UdpPeer& client) {
        ASSERT_TRUE(client.user_data());

        const auto& data_str = *reinterpret_cast<std::string*>(client.user_data());
        peer_to_close_count[data_str]++;
    });

    auto client_1 = new io::UdpClient(loop, 0x7F000001, m_default_port);
    io::Timer timer_1(loop);
    timer_1.start(0, 100, [&](io::Timer& timer) {
        client_1->send_data(client_1_message,
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
            }
        );
    });

    auto client_2 = new io::UdpClient(loop, 0x7F000001, m_default_port);
    io::Timer timer_2(loop);
    timer_2.start(0, 200, [&](io::Timer& timer) {
        client_2->send_data(client_2_message,
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
            }
        );
    });

    auto client_3 = new io::UdpClient(loop, 0x7F000001, m_default_port);
    io::Timer timer_3(loop);
    timer_3.start(0, 400, [&](io::Timer& timer) {
        client_3->send_data(client_3_message,
            [&](io::UdpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
            }
        );
    });

    io::Timer timer_stop_all(loop);
    timer_stop_all.start(650, [&](io::Timer& timer) {
        timer_1.stop();
        timer_2.stop();
        timer_3.stop();
    });

    io::Timer timer_remove(loop);
    timer_remove.start(1000, [&](io::Timer& timer) {
        client_1->schedule_removal();
        client_2->schedule_removal();
        client_3->schedule_removal();
        server->schedule_removal();
    });

    EXPECT_EQ(0, peer_to_close_count.size());

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(3, peer_to_close_count.size());
    EXPECT_EQ(1, peer_to_close_count[client_1_message]);
    EXPECT_EQ(1, peer_to_close_count[client_2_message]);
    EXPECT_EQ(2, peer_to_close_count[client_3_message]);
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
    ASSERT_EQ(io::Error(0), server->bind(m_default_addr, m_default_port));
    server->start_receive([&](io::UdpServer& server, io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        ++server_data_receive_counter;

        std::string s(data.buf.get(), data.size);
        EXPECT_EQ(client_message, s);

        peer.send_data(server_message,
            [&](io::UdpPeer& peer, const io::Error& error) {
                EXPECT_FALSE(error) << error.string();
                ++server_data_send_counter;
                server.schedule_removal();
            }
        );
    });

    auto client = new io::UdpClient(loop, 0x7F000001, m_default_port,
        [&](io::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            std::string s(data.buf.get(), data.size);
            EXPECT_EQ(server_message, s);

            ++client_data_receive_counter;
            client.schedule_removal();
        }
    );

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

    ASSERT_EQ(0, loop.run());

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
            if (server_data_send_counter < 2) {
                ++server_data_send_counter;
                client.send_data(server_message[server_data_send_counter], on_server_send);
            } else {
                server->schedule_removal();
            }
        };

    ASSERT_EQ(io::Error(0), server->bind(m_default_addr, m_default_port));
    server->start_receive([&](io::UdpServer& server, io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        ++server_data_receive_counter;

        peer.send_data(server_message[server_data_send_counter], on_server_send);
    });

    auto client = new io::UdpClient(loop, 0x7F000001, m_default_port,
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

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, server_data_receive_counter);
    EXPECT_EQ(2, server_data_send_counter);
    EXPECT_EQ(2, client_data_receive_counter);
    EXPECT_EQ(1, client_data_send_counter);
}

TEST_F(UdpClientServerTest, client_receive_data_only_from_it_target) {
    io::EventLoop loop;

    const std::string client_message = "I am client";
    const std::string other_message = "Spam!Spam!Spam!";

    std::size_t receive_callback_call_count = 0;

    auto client_2 = new io::UdpClient(loop);

    auto client_1 = new io::UdpClient(loop,
        [&](io::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++receive_callback_call_count;
        }
    );
    client_1->set_destination(0x7F000001, m_default_port);
    client_1->send_data(client_message,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            // Note: Client1 bound port is only known when it make some activity like sending data
            client_2->set_destination(0x7F000001, client.bound_port());
            client_2->send_data(other_message,
                [&](io::UdpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                }
            );
        }
    );

    io::Timer timer(loop);
    timer.start(100, [&](io::Timer&) {
        client_1->schedule_removal();
        client_2->schedule_removal();
    });

    EXPECT_EQ(0, receive_callback_call_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(0, receive_callback_call_count);
}

TEST_F(UdpClientServerTest, send_larger_than_ethernet_mtu) {
    io::EventLoop loop;

    std::size_t SIZE = 5000;

    bool data_sent = false;
    bool data_received = false;

    auto server = new io::UdpServer(loop);
    ASSERT_EQ(io::Error(0), server->bind(m_default_addr, m_default_port));
    server->start_receive([&](io::UdpServer& server, io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        EXPECT_EQ(SIZE, data.size);
        data_received = true;

        for (size_t i = 0; i < SIZE / 2; ++i) {
            ASSERT_EQ(i, *(reinterpret_cast<const std::uint16_t*>(data.buf.get()) + i))  << "i =" << i;
        }

        server.schedule_removal();
    });

    std::shared_ptr<char> message(new char[SIZE], std::default_delete<char[]>());
    for (size_t i = 0; i < SIZE / 2; ++i) {
        *(reinterpret_cast<std::uint16_t*>(message.get()) + i) = i;
    }

    auto client = new io::UdpClient(loop);
    client->set_destination(0x7F000001, m_default_port);
    client->send_data(message, SIZE,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            data_sent = true;
            client.schedule_removal();
        }
    );

    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(data_sent);
    EXPECT_TRUE(data_received);
}

TEST_F(UdpClientServerTest, send_larger_than_allowed_to_send) {
    io::EventLoop loop;

    std::size_t SIZE = 100 * 1024;

    std::shared_ptr<char> message(new char[SIZE], std::default_delete<char[]>());
    std::memset(message.get(), 0, SIZE);

    auto client = new io::UdpClient(loop, 0x7F000001, m_default_port);
    client->send_data(message, SIZE,
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::MESSAGE_TOO_LONG, error.code());
            client.schedule_removal();
        }
    );

    ASSERT_EQ(0, loop.run());
}

// TODO: this test with parallel send of all data
TEST_F(UdpClientServerTest, client_and_server_exchange_lot_of_data) {
    // Note: as we perform this test on local host on the same event loop, we expect that data
    // will be received sequentially
    io::EventLoop loop;

    std::size_t SIZE = 5000;
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
    ASSERT_EQ(io::Error(0), server->bind(m_default_addr, m_default_port));
    server->start_receive(
        [&](io::UdpServer& server, io::UdpPeer& client, const io::DataChunk& chunk, const io::Error& error) {
            EXPECT_FALSE(error);

            for (std::size_t i = 0; i < SIZE - server_receive_message_counter; ++i) {
                ASSERT_EQ(message.get()[i], chunk.buf.get()[i]) << "i= " << i;
            }

            if (!server_send_started) {
                client.send_data(message, SIZE - client_send_message_counter, server_send);
                server_send_started = true;
            }

            ++server_receive_message_counter;
        }
    );

    std::function<void(io::UdpClient&, const io::Error&)> client_send =
        [&](io::UdpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_send_message_counter;
            if (client_send_message_counter < SIZE) {
                client.send_data(message, SIZE - client_send_message_counter, client_send);
            }
        };

    auto client = new io::UdpClient(loop, 0x7F000001, m_default_port,
        [&](io::UdpClient&, const io::DataChunk& chunk, const io::Error& error) {
            EXPECT_FALSE(error);
            for (std::size_t i = 0; i < SIZE - client_receive_message_counter; ++i) {
                ASSERT_EQ(message.get()[i], chunk.buf.get()[i]) << "i= " << i;
            }

            ++client_receive_message_counter;
        }
    );
    client->send_data(message, SIZE - client_send_message_counter, client_send);

    io::Timer timer(loop);
    timer.start(100, 100, [&](io::Timer& timer) {
        if (client_send_message_counter == SIZE && server_send_message_counter == SIZE) {
            client->schedule_removal();
            server->schedule_removal();
            timer.stop();
        }
    });

    ASSERT_EQ(0, loop.run());
}

// TODO: client and server in threads

// TODO: UDP client sending test with no destination set
// TODO: check address of UDP peer
// TODO: error on multiple start_receive on UDP server

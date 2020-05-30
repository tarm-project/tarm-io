/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "io/TcpClient.h"
#include "io/TcpServer.h"
#include "io/ScopeExitGuard.h"
#include "io/Timer.h"

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

struct TcpClientServerTest : public testing::Test,
                             public LogRedirector {

protected:
    std::uint16_t m_default_port = 31540;
    std::string m_default_addr = "127.0.0.1";

    void test_impl_server_disconnect_client_from_data_receive_callback(
        std::function<void(io::TcpConnectedClient& client)> close_variant);

    void test_impl_client_double_close(
        std::function<void(io::TcpClient& client)> close_variant);

    void test_impl_server_double_close(
        std::function<void(io::TcpServer& server,
                           std::function<void(io::TcpServer&, const io::Error& error)> callback)> close_variant);
};

TEST_F(TcpClientServerTest, server_constructor) {
    io::EventLoop loop;
    auto server = new io::TcpServer(loop);
    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TcpClientServerTest, client_constructor) {
    io::EventLoop loop;
    auto client = new io::TcpClient(loop);
    client->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TcpClientServerTest, invalid_ip4_address) {
    io::EventLoop loop;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({"1234567890", m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        },
        nullptr
    );
    EXPECT_TRUE(listen_error);
    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, listen_error.code());

    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TcpClientServerTest, schedule_removal_not_connected_client) {
    io::EventLoop loop;
    auto client = new io::TcpClient(loop);
    client->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

#if defined(__APPLE__) || defined(__linux__)
// Windows does not have privileged ports
TEST_F(TcpClientServerTest, bind_privileged) {
    io::EventLoop loop;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({m_default_addr, 100},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        },
        nullptr
    );
    EXPECT_TRUE(listen_error);
    EXPECT_EQ(io::StatusCode::PERMISSION_DENIED, listen_error.code());

    server->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}
#endif

TEST_F(TcpClientServerTest, server_address_in_use) {
    io::EventLoop loop;

    auto server_1 = new io::TcpServer(loop);
    auto listen_error_1 = server_1->listen({m_default_addr, m_default_port}, nullptr, nullptr, nullptr);
    EXPECT_FALSE(listen_error_1);

    auto server_2 = new io::TcpServer(loop);
    auto listen_error_2 = server_2->listen({m_default_addr, m_default_port}, nullptr, nullptr, nullptr);
    EXPECT_TRUE(listen_error_2);
    EXPECT_EQ(io::StatusCode::ADDRESS_ALREADY_IN_USE, listen_error_2.code());

    server_1->schedule_removal();
    server_2->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TcpClientServerTest, client_connect_to_invalid_address) {
    io::EventLoop loop;

    std::size_t client_on_connect_count = 0;

    auto client = new io::TcpClient(loop);
    client->connect({"0.0.0", m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
            EXPECT_FALSE(client.is_open());

            ++client_on_connect_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_on_connect_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_connect_count);
}

TEST_F(TcpClientServerTest, 1_client_sends_data_to_server) {
    io::EventLoop loop;

    const std::string message = "Hello world!";

    bool data_received = false;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
    [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
        const auto& endpoint = client.endpoint();
        ASSERT_EQ(io::Endpoint::IP_V4, endpoint.type());
        EXPECT_EQ(endpoint.ipv4_addr(), 0x7F000001);
        EXPECT_NE(endpoint.port(), 0);
        EXPECT_NE(endpoint.port(), m_default_port);
    },
    [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_TRUE(client.is_open());

        const auto& endpoint = client.endpoint();
        ASSERT_EQ(io::Endpoint::IP_V4, endpoint.type());
        EXPECT_EQ(endpoint.ipv4_addr(), 0x7F000001);
        EXPECT_NE(endpoint.port(), 0);
        EXPECT_NE(endpoint.port(), m_default_port);

        data_received = true;
        std::string received_message(data.buf.get(), data.size);

        EXPECT_EQ(data.size, message.length());
        EXPECT_EQ(received_message, message);

        server->schedule_removal();
    },
    nullptr);
    ASSERT_FALSE(listen_error);

    bool data_sent = false;

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
    [&](io::TcpClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
        EXPECT_TRUE(client.is_open());

        client.send_data(message, [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_TRUE(client.is_open());
            data_sent = true;
            client.schedule_removal();
        });
    },
    nullptr);

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_TRUE(data_sent);
    EXPECT_TRUE(data_received);
}

TEST_F(TcpClientServerTest, 2_clients_send_data_to_server) {
    io::EventLoop loop;

    const std::string message = "Hello world!";

    bool data_received_1 = false;
    bool data_received_2 = false;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
    [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
    },
    [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_EQ(data.size, message.length() + 1);

        std::string received_message(data.buf.get(), data.size);

        if (message + "1" == received_message) {
            data_received_1 = true;
        } else if (message + "2" == received_message) {
            data_received_2 = true;
        }

        if (data_received_1 && data_received_2) {
            server->schedule_removal();
        }
    },
    nullptr);
    EXPECT_FALSE(listen_error);

    bool data_sent_1 = false;

    auto client_1 = new io::TcpClient(loop);
    client_1->connect({m_default_addr, m_default_port}, [&](io::TcpClient& client, const io::Error& error) {
        EXPECT_FALSE(error);

        client.send_data(message + "1", [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            data_sent_1 = true;
            client.schedule_removal();
        });
    },
    nullptr);

    bool data_sent_2 = false;

    auto client_2 = new io::TcpClient(loop);
    client_2->connect({m_default_addr, m_default_port}, [&](io::TcpClient& client, const io::Error& error) {
        EXPECT_FALSE(error);

        client.send_data(message + "2", [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            data_sent_2 = true;
            client.schedule_removal();
        });
    },
    nullptr);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_TRUE(data_sent_1);
    EXPECT_TRUE(data_sent_2);
    EXPECT_TRUE(data_received_1);
    EXPECT_TRUE(data_received_2);
}

TEST_F(TcpClientServerTest, client_and_server_in_threads) {
    const std::string message = "Hello!";

    bool send_called = false;
    bool receive_called = false;

    std::thread server_thread([this, &message, &receive_called]() {
        io::EventLoop loop;
        auto server = new io::TcpServer(loop);
        auto listen_error = server->listen({"0.0.0.0", m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_EQ(data.size, message.length());
            EXPECT_EQ(std::string(data.buf.get(), data.size), message);
            receive_called = true;
            server->schedule_removal();
        },
        nullptr);
        EXPECT_FALSE(listen_error);

        EXPECT_EQ(io::StatusCode::OK, loop.run());
        EXPECT_TRUE(receive_called);
    });

    std::thread client_thread([this, &message, &send_called](){
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        io::EventLoop loop;
        auto client = new io::TcpClient(loop);
        client->connect({m_default_addr, m_default_port}, [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            client.send_data(message, [&](io::TcpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                send_called = true;
                client.schedule_removal();
            });
        },
        nullptr);

        EXPECT_EQ(io::StatusCode::OK, loop.run());
        EXPECT_TRUE(send_called);
    });

    io::ScopeExitGuard guard([&server_thread, &client_thread](){
        client_thread.join();
        server_thread.join();
    });
}

TEST_F(TcpClientServerTest, server_sends_data_first) {
    io::EventLoop loop;

    const std::string message = "Hello world!";

    bool data_sent = false;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
    [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
        client.send_data(message, [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            data_sent = true;
        });
    },
    [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        server->schedule_removal(); // receive 'shutdown' message
    },
    nullptr);
    EXPECT_FALSE(listen_error);

    bool data_received = false;

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port}, [&](io::TcpClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
        // do not send data
    },
    [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        EXPECT_EQ(data.size, message.length());
        EXPECT_EQ(std::string(data.buf.get(), data.size), message);
        data_received = true;

        client.send_data("shutdown", [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.schedule_removal();
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_TRUE(data_sent);
    EXPECT_TRUE(data_received);
}

TEST_F(TcpClientServerTest, multiple_data_chunks_sent_in_a_row_by_client) {
    // Note: in this test verifying that data send is queued and delivered in order

    const std::size_t CHUNK_SIZE = 65536;
    const std::size_t CHUNKS_COUNT = 20;
    const std::size_t TOTAL_BYTES = CHUNK_SIZE * CHUNKS_COUNT;

    ASSERT_EQ(0, CHUNK_SIZE % sizeof(std::uint32_t));

    std::thread server_thread([this, TOTAL_BYTES]() {
        io::EventLoop loop;
        auto server = new io::TcpServer(loop);

        std::size_t bytes_received = 0;

        std::vector<unsigned char> total_bytes_received;

        auto listen_error = server->listen({"0.0.0.0", m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            total_bytes_received.insert(total_bytes_received.end(), data.buf.get(), data.buf.get() + data.size);
            bytes_received += data.size;
            if (bytes_received >= TOTAL_BYTES) {
                server->schedule_removal();
            }
        },
        nullptr);
        EXPECT_FALSE(listen_error);

        EXPECT_EQ(io::StatusCode::OK, loop.run());
        EXPECT_EQ(TOTAL_BYTES, bytes_received);
        EXPECT_EQ(TOTAL_BYTES, total_bytes_received.size());
        for (std::size_t i = 0; i < total_bytes_received.size(); i += 4) {
            ASSERT_EQ(i / 4, *reinterpret_cast<std::uint32_t*>((&total_bytes_received[0] + i)));
        }
    });

    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::size_t counter = 0;
        std::function<void(io::TcpClient&, const io::Error&)> on_data_sent = [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            if (++counter == CHUNKS_COUNT) {
                client.schedule_removal();
            }
        };

        io::EventLoop loop;
        auto client = new io::TcpClient(loop);
        client->connect({m_default_addr, m_default_port}, [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            for (size_t i = 0; i < CHUNKS_COUNT; ++i) {
                std::shared_ptr<char> buf(new char[CHUNK_SIZE], [](const char* p) { delete[] p; });
                for (std::size_t k = 0; k < CHUNK_SIZE; k += 4) {
                    (*reinterpret_cast<std::uint32_t*>(buf.get() + k) ) = k / 4 + i * CHUNK_SIZE / 4;
                }
                client.send_data(buf, CHUNK_SIZE, on_data_sent);
            }
        },
        nullptr);

        EXPECT_EQ(io::StatusCode::OK, loop.run());
    });

    io::ScopeExitGuard guard([&server_thread, &client_thread](){
        client_thread.join();
        server_thread.join();
    });
}

TEST_F(TcpClientServerTest, null_send_buf) {
    io::EventLoop loop;

    std::shared_ptr<char> buf;

    std::size_t server_on_send_count = 0;
    std::size_t client_on_send_count = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            client.send_data(buf, 1,
                [&](io::TcpConnectedClient& client, const io::Error& error) {
                    EXPECT_TRUE(error);
                    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
                    ++server_on_send_count;
                    server->schedule_removal();
                }
            );
        },
        nullptr,
        nullptr
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            client.send_data(buf, 1,
                [&](io::TcpClient& client, const io::Error& error) {
                    EXPECT_TRUE(error);
                    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
                    ++client_on_send_count;
                    client.schedule_removal();
                }
            );
        },
        nullptr
    );

    EXPECT_EQ(0, server_on_send_count);
    EXPECT_EQ(0, client_on_send_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_send_count);
    EXPECT_EQ(1, client_on_send_count);
}

TEST_F(TcpClientServerTest, server_shutdown_callback) {
    io::EventLoop loop;

    const std::string client_message = "Hello world!";

    std::size_t on_server_shutdown_call_count = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
    [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
    },
    [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);

        server->shutdown([&](io::TcpServer& server, const io::Error& error) {
            EXPECT_FALSE(error);
            ++on_server_shutdown_call_count;
            server.schedule_removal();
        });
    },
    nullptr);

    ASSERT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
    [&](io::TcpClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
        client.send_data(client_message, [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.schedule_removal();
        });
    },
    nullptr);

    EXPECT_EQ(0, on_server_shutdown_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_server_shutdown_call_count);
}

TEST_F(TcpClientServerTest, server_close_callback_1) {
    std::size_t on_server_close_call_count = 0;

    io::EventLoop loop;

    auto server = new io::TcpServer(loop);
    server->close([&](io::TcpServer& server, const io::Error& error) {
        EXPECT_TRUE(error);
        EXPECT_EQ(io::StatusCode::NOT_CONNECTED, error.code());
        ++on_server_close_call_count; // TODO: if put this line after schedule_removal it corrupts memory (see valgrind)
        server.schedule_removal();
    });

    EXPECT_EQ(0, on_server_close_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_server_close_call_count);
}

TEST_F(TcpClientServerTest, server_close_callback_2) {
    std::size_t on_server_close_call_count = 0;

    io::EventLoop loop;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
    [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
    },
    [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
    },
    nullptr);
    ASSERT_FALSE(listen_error);

    server->close([&](io::TcpServer& server, const io::Error& error) {
        EXPECT_FALSE(error);
        ++on_server_close_call_count;
        server.schedule_removal();
    });

    EXPECT_EQ(0, on_server_close_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_server_close_call_count);
}

TEST_F(TcpClientServerTest, server_close_callback_3) {
    io::EventLoop loop;

    const std::string client_message = "Hello world!";

    std::size_t on_server_close_call_count = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
    [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
    },
    [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);

        server->close([&](io::TcpServer& server, const io::Error& error) {
            EXPECT_FALSE(error);
            ++on_server_close_call_count;
            server.schedule_removal();
        });
    },
    nullptr);

    ASSERT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
    [&](io::TcpClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
        client.send_data(client_message, [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.schedule_removal();
        });
    },
    nullptr);

    EXPECT_EQ(0, on_server_close_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_server_close_call_count);
}

TEST_F(TcpClientServerTest, client_connect_to_nonexistent_server) {
    bool callback_called = false;

    io::EventLoop loop;
    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port}, [&](io::TcpClient& client, const io::Error& error) {
        EXPECT_TRUE(error);
        EXPECT_FALSE(client.is_open());
        EXPECT_EQ(io::StatusCode::CONNECTION_REFUSED, error.code());
        callback_called = true;


        client.schedule_removal();
    },
    nullptr);

    EXPECT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_TRUE(callback_called);
}

TEST_F(TcpClientServerTest, server_disconnect_client_from_new_connection_callback) {
    io::EventLoop loop;
    auto server = new io::TcpServer(loop);

    std::size_t server_new_connection_callback_call_count = 0;
    std::size_t server_receive_callback_call_count = 0;
    std::size_t client_receive_callback_call_count = 0;
    std::size_t client_close_callback_call_count = 0;

    const std::string client_message = "Hello :-)";
    const std::string server_message = "Go away!";

    std::vector<unsigned char> total_bytes_received;

    auto listen_error = server->listen({"0.0.0.0", m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_new_connection_callback_call_count;

            client.send_data(server_message,
                [&](io::TcpConnectedClient&, const io::Error& error) {
                    EXPECT_FALSE(error);
                }
            );

            EXPECT_EQ(1, server->connected_clients_count());
            client.shutdown();
            EXPECT_EQ(1, server->connected_clients_count());
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            EXPECT_TRUE(client.is_open());
            ++server_receive_callback_call_count;
        },
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            EXPECT_EQ(0, server->connected_clients_count());
        }
    );
    EXPECT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            client.send_data(client_message);
            client.send_data(client_message); // Should not be received
        },
        [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            EXPECT_EQ(std::string(data.buf.get(), data.size), server_message);
            ++client_receive_callback_call_count;
        },
        [&](io::TcpClient& client, const io::Error& error) {
            // Not checking status here bacause could be connection reset error
            ++client_close_callback_call_count;

            client.schedule_removal();
            server->schedule_removal();
        }
    );

    EXPECT_EQ(0, server_new_connection_callback_call_count);
    EXPECT_EQ(0, server_receive_callback_call_count);
    EXPECT_EQ(0, client_receive_callback_call_count);
    EXPECT_EQ(0, client_close_callback_call_count);

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_new_connection_callback_call_count);
    EXPECT_EQ(0, server_receive_callback_call_count);
    EXPECT_EQ(1, client_receive_callback_call_count);
    EXPECT_EQ(1, client_close_callback_call_count);
}

TEST_F(TcpClientServerTest, server_close_calls_close_on_connected_clients) {
    // Test description: in this test we check that close callbacks are called
    // for both client and server side when server is closed

    io::EventLoop loop;
    auto server = new io::TcpServer(loop);

    unsigned server_connect_callback_count = 0;
    unsigned server_receive_callback_count = 0;
    unsigned server_send_callback_count = 0;
    unsigned client_receive_callback_count = 0;
    unsigned client_close_callback_count = 0;
    unsigned connected_client_close_callback_count = 0;

    const std::string server_message = "I quit!";

    std::function<void(io::TcpConnectedClient&, const io::Error&)> connected_client_close_callback =
        [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
        ++connected_client_close_callback_count;
    };

    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_connect_callback_count;
            client.send_data(server_message, [&](io::TcpConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                ++server_send_callback_count;
                server->close([&](io::TcpServer& server, const io::Error& error) {
                    EXPECT_FALSE(error);
                    server.schedule_removal();
                });
            });
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_receive_callback_count;
        },
        connected_client_close_callback
    );
    EXPECT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_receive_callback_count;
            client.schedule_removal();
        },
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_close_callback_count;
        }
    );

    EXPECT_EQ(0, server_connect_callback_count);
    EXPECT_EQ(0, server_receive_callback_count);
    EXPECT_EQ(0, server_send_callback_count);
    EXPECT_EQ(0, client_receive_callback_count);
    EXPECT_EQ(0, connected_client_close_callback_count);
    EXPECT_EQ(0, client_close_callback_count);

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_connect_callback_count);
    EXPECT_EQ(0, server_receive_callback_count);
    EXPECT_EQ(1, server_send_callback_count);
    EXPECT_EQ(1, client_receive_callback_count);
    EXPECT_EQ(1, connected_client_close_callback_count);
    EXPECT_EQ(1, client_close_callback_count);
}

TEST_F(TcpClientServerTest, server_shutdown_calls_close_on_connected_clients) {
    // Test description: in this test we check that close callbacks are called
    // for both client and server side when server is shut down

    io::EventLoop loop;
    auto server = new io::TcpServer(loop);

    unsigned server_connect_callback_count = 0;
    unsigned server_receive_callback_count = 0;
    unsigned server_send_callback_count = 0;
    unsigned client_receive_callback_count = 0;
    unsigned client_close_callback_count = 0;
    unsigned connected_client_close_callback_count = 0;

    const std::string server_message = "I quit!";

    std::function<void(io::TcpConnectedClient&, const io::Error&)> connected_client_close_callback =
        [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
        ++connected_client_close_callback_count;
    };

    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_connect_callback_count;
            client.send_data(server_message, [&](io::TcpConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                ++server_send_callback_count;
                server->shutdown([&](io::TcpServer& server, const io::Error& error) {
                    EXPECT_FALSE(error);
                    server.schedule_removal();
                });
            });
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_receive_callback_count;
        },
        connected_client_close_callback
    );
    EXPECT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_receive_callback_count;
            client.schedule_removal();
        },
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_close_callback_count;
        }
    );

    EXPECT_EQ(0, server_connect_callback_count);
    EXPECT_EQ(0, server_receive_callback_count);
    EXPECT_EQ(0, server_send_callback_count);
    EXPECT_EQ(0, client_receive_callback_count);
    EXPECT_EQ(0, connected_client_close_callback_count);
    EXPECT_EQ(0, client_close_callback_count);

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_connect_callback_count);
    EXPECT_EQ(0, server_receive_callback_count);
    EXPECT_EQ(1, server_send_callback_count);
    EXPECT_EQ(1, client_receive_callback_count);
    EXPECT_EQ(1, connected_client_close_callback_count);
    EXPECT_EQ(1, client_close_callback_count);
}

TEST_F(TcpClientServerTest, close_in_server_on_close_callback) {
    io::EventLoop loop;

    std::size_t on_server_close_call_count = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            std::string message(data.buf.get(), data.size);
            if (message == "close") {
                client.close();
            } else {
                client.shutdown();
            }
        },
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++on_server_close_call_count;
            client.close();
        }
    );
    ASSERT_FALSE(listen_error) << listen_error.string();

    auto client_on_connect = [&](io::TcpClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
        client.send_data(client.user_data_as_ptr<char>(),
                [&](io::TcpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                client.schedule_removal();
            }
        );
    };

    auto client_1 = new io::TcpClient(loop);
    std::string command_1("close");
    client_1->set_user_data(&command_1[0]);
    client_1->connect({m_default_addr, m_default_port},
        client_on_connect,
        nullptr
    );

    auto client_2 = new io::TcpClient(loop);
    std::string command_2("shutdown");
    client_2->set_user_data(&command_2[0]);
    client_2->connect({m_default_addr, m_default_port},
        client_on_connect,
        nullptr
    );

    (new io::Timer(loop))->start(100,
        [&](io::Timer& timer) {
            server->schedule_removal();
            timer.schedule_removal();
        }
    );

    EXPECT_EQ(0, on_server_close_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(2, on_server_close_call_count);
}

TEST_F(TcpClientServerTest, server_schedule_remove_after_send) {
    const std::string server_message = "I quit!";

    std::size_t server_send_callback_count = 0;
    std::size_t server_receive_callback_count = 0;
    std::size_t client_receive_callback_count = 0;

    io::EventLoop loop;
    auto server = new io::TcpServer(loop);

    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            //++server_connect_callback_count;
            client.send_data(server_message,
                [&](io::TcpConnectedClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    ++server_send_callback_count;
                    client.server().schedule_removal();
                }
            );
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            ++server_receive_callback_count;
        },
        nullptr
    );
    EXPECT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_receive_callback_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_send_callback_count);
    EXPECT_EQ(0, server_receive_callback_count);
    EXPECT_EQ(0, client_receive_callback_count);

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_send_callback_count);
    EXPECT_EQ(0, server_receive_callback_count);
    EXPECT_EQ(1, client_receive_callback_count);
}

void TcpClientServerTest::test_impl_server_disconnect_client_from_data_receive_callback(
    std::function<void(io::TcpConnectedClient& client)> close_variant) {
    const std::string client_message = "Disconnect me!";

    io::EventLoop loop;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({m_default_addr, m_default_port},
    [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error) << error.string();
    },
    [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_TRUE(client.is_open());
        EXPECT_FALSE(error) << error.string();
        EXPECT_EQ(std::string(data.buf.get(), data.size), client_message);
        close_variant(client);
        EXPECT_EQ(1, server->connected_clients_count()); // not closed yet
    },
    [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error) << error.string();
        EXPECT_EQ(0, server->connected_clients_count());
    });
    EXPECT_FALSE(listen_error);

    bool disconnect_called = false;

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data(client_message);
        },
        [](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(client.is_open());
            disconnect_called = true;
        });

    auto timer = new io::Timer(loop);
    timer->start(500, [&](io::Timer& timer) {
        // Disconnect should be called before timer callback
        EXPECT_TRUE(disconnect_called);

        client->schedule_removal();
        server->schedule_removal();
        timer.schedule_removal();
    });

    EXPECT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_TRUE(disconnect_called);
}

TEST_F(TcpClientServerTest, server_disconnect_client_from_data_receive_callback_1) {
    test_impl_server_disconnect_client_from_data_receive_callback(
        [](io::TcpConnectedClient& client) {
            client.close();
        }
    );
}

TEST_F(TcpClientServerTest, server_disconnect_client_from_data_receive_callback_2) {
    test_impl_server_disconnect_client_from_data_receive_callback(
        [](io::TcpConnectedClient& client) {
            client.shutdown();
        }
    );
}

TEST_F(TcpClientServerTest, connect_and_simultaneous_send_many_participants) {
    io::EventLoop server_loop;

    std::size_t server_on_connect_counter = 0;
    std::size_t server_on_data_receive_counter = 0;
    std::size_t server_on_close_counter = 0;

    const std::size_t NUMBER_OF_CLIENTS = 100;
    std::vector<bool> clinets_data_log(NUMBER_OF_CLIENTS, false);

    auto server = new io::TcpServer(server_loop);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_connect_counter;
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_data_receive_counter;
            ASSERT_EQ(sizeof(std::size_t), data.size);
            const auto value = *reinterpret_cast<const std::size_t*>(data.buf.get());
            ASSERT_LT(value, clinets_data_log.size());
            clinets_data_log[value] = true;

            if (server_on_data_receive_counter == NUMBER_OF_CLIENTS) {
                client.server().shutdown([&](io::TcpServer& server, const io::Error& error) {
                    EXPECT_EQ(NUMBER_OF_CLIENTS, server_on_close_counter);
                    EXPECT_FALSE(error);
                    server.schedule_removal();
                });
            }
        },
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_close_counter;
        }
    );
    EXPECT_FALSE(listen_error);

    std::thread client_thread([this, NUMBER_OF_CLIENTS]() {
        io::EventLoop clients_loop;

        std::vector<io::TcpClient*> all_clients;
        all_clients.reserve(NUMBER_OF_CLIENTS);

        auto send_all_clients = [&all_clients]() {
            for(std::size_t i = 0; i < all_clients.size(); ++i) {
                auto data_value = reinterpret_cast<std::size_t>(all_clients[i]->user_data());
                std::shared_ptr<char> data(new char[sizeof(std::size_t)], [](const char* p) { delete[] p;});
                *reinterpret_cast<std::size_t*>(data.get()) = data_value;
                all_clients[i]->send_data(data, sizeof(std::size_t),
                    [](io::TcpClient& client, const io::Error& error) {
                        EXPECT_FALSE(error);
                        client.schedule_removal();
                    });
            }
        };

        for(std::size_t i = 0; i < NUMBER_OF_CLIENTS; ++i) {
            auto tcp_client = new io::TcpClient(clients_loop);
            tcp_client->set_user_data(reinterpret_cast<void*>(i));
            tcp_client->connect({m_default_addr, m_default_port},
                [i, tcp_client, NUMBER_OF_CLIENTS, send_all_clients, &all_clients]
                (io::TcpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    all_clients.push_back(tcp_client);

                    if (i == NUMBER_OF_CLIENTS - 1) {
                        ASSERT_EQ(NUMBER_OF_CLIENTS, all_clients.size());
                        send_all_clients();
                    }
                },
            nullptr);
        }

        EXPECT_EQ(io::StatusCode::OK, clients_loop.run());
    });

    io::ScopeExitGuard guard([&client_thread](){
        client_thread.join();
    });

    EXPECT_EQ(0, server_on_connect_counter);
    EXPECT_EQ(0, server_on_data_receive_counter);

    EXPECT_EQ(io::StatusCode::OK, server_loop.run());

    EXPECT_EQ(NUMBER_OF_CLIENTS, server_on_connect_counter);
    EXPECT_EQ(NUMBER_OF_CLIENTS, server_on_data_receive_counter);

    for(std::size_t i = 0; i < NUMBER_OF_CLIENTS; ++i) {
        ASSERT_TRUE(clinets_data_log[i]) << " i= " << i;
    }
}

TEST_F(TcpClientServerTest, client_disconnects_from_server) {
    io::EventLoop loop;

    auto server = new io::TcpServer(loop);

    std::size_t on_connected_client_close_call_count = 0;
    std::size_t on_raw_client_close_call_count = 0;

    io::TcpServer::NewConnectionCallback on_server_new_connection =
        [=](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        };

    io::TcpServer::DataReceivedCallback on_server_receive_data =
        [](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {

        };

    io::TcpServer::CloseConnectionCallback on_connected_client_close =
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++on_connected_client_close_call_count;
        };

    auto listen_error = server->listen({m_default_addr, m_default_port},
                                       on_server_new_connection,
                                       on_server_receive_data,
                                       on_connected_client_close);
    EXPECT_FALSE(listen_error);

    io::TcpClient::ConnectCallback on_client_connect_callback =
        [](io::TcpClient& client, const io::Error error) {
            client.close();
        };
    io::TcpClient::CloseCallback on_raw_client_close_callback =
        [&](io::TcpClient& client, const io::Error error) {
            ++on_raw_client_close_call_count;
        };

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
                    on_client_connect_callback,
                    nullptr,
                    on_raw_client_close_callback);

    auto timer = new io::Timer(loop);
    timer->start(500, [&](io::Timer& timer) {
        // Disconnect should be called before timer callback
        EXPECT_EQ(1, on_connected_client_close_call_count);

        client->schedule_removal();
        server->schedule_removal();
        timer.schedule_removal();
    });

    EXPECT_EQ(0, on_connected_client_close_call_count);
    EXPECT_EQ(0, on_raw_client_close_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_connected_client_close_call_count);
    EXPECT_EQ(1, on_raw_client_close_call_count);
}

TEST_F(TcpClientServerTest, client_closed_without_read_and_connect_callback) {
    io::EventLoop loop;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen(
        io::Endpoint{m_default_addr, m_default_port},
        [](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.close();
            client.server().schedule_removal();
        },
        nullptr,
        nullptr
    );
    EXPECT_FALSE(listen_error);

    int client_close_call_count = 0;

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        nullptr,
        nullptr,
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_close_call_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_close_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_close_call_count);
}

TEST_F(TcpClientServerTest, server_shutdown_makes_client_close) {
    // Note: in this test we send data in both direction and shout down server
    // from the on_send callback. Client is removed from clase callback which is
    // executed on server shoutdown.

    io::EventLoop loop;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            client.send_data("world", [](io::TcpConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
            });

            client.send_data("!", [](io::TcpConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error);

                client.server().shutdown([&](io::TcpServer& server, const io::Error& error) {
                    EXPECT_FALSE(error);
                    client.server().schedule_removal();
                });
            });
        },
        nullptr
    );
    EXPECT_FALSE(listen_error);

    int client_connect_call_count = 0;
    int client_close_call_count = 0;

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            ++client_connect_call_count;
            EXPECT_FALSE(error);
            client.send_data("Hello ",
                [&](io::TcpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                }
            );
        },
        [](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_close_call_count;
            client.schedule_removal();
        }
    );

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_EQ(1, client_connect_call_count);
    EXPECT_EQ(1, client_close_call_count);
}

TEST_F(TcpClientServerTest, pending_write_requests) {
    io::EventLoop loop;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen(
        io::Endpoint{m_default_addr, m_default_port},
        [](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_EQ(0, client.pending_write_requesets());
            client.send_data("world", [](io::TcpConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                EXPECT_EQ(1, client.pending_write_requesets());
            });
            EXPECT_EQ(1, client.pending_write_requesets());
            client.send_data("!", [](io::TcpConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                EXPECT_EQ(0, client.pending_write_requesets());

                client.server().shutdown([&](io::TcpServer& server, const io::Error& error) {
                    EXPECT_FALSE(error);
                    server.schedule_removal();
                });
            });
            EXPECT_EQ(2, client.pending_write_requesets());
        },
        nullptr
    );
    EXPECT_FALSE(listen_error);

    int client_connect_call_count = 0;
    int client_close_call_count = 0;

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            EXPECT_EQ(0, client.pending_write_requesets());
            ++client_connect_call_count;

            client.send_data("H");
            client.send_data("e");
            client.send_data("l");
            client.send_data("l");
            client.send_data("o");
            client.send_data(" ",
                [&](io::TcpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    EXPECT_EQ(0, client.pending_write_requesets());
                }
            );
            EXPECT_EQ(6, client.pending_write_requesets());
        },
        [](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        [&](io::TcpClient& client, const io::Error& error) { // on close
            EXPECT_FALSE(error) << error.string();
            ++client_close_call_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_connect_call_count);
    EXPECT_EQ(0, client_close_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_connect_call_count);
    EXPECT_EQ(1, client_close_call_count);
}

TEST_F(TcpClientServerTest, client_shutdown_in_connect) {
    io::EventLoop loop;

    const std::string message = "Hello world!";

    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;
    std::size_t client_on_close_count = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
    [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
    },
    [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        ++server_on_receive_count;
    },
    [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
        ++server_on_close_count;
        server->schedule_removal();
    });

    ASSERT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr,m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_TRUE(client.is_open());

            client.shutdown();
        },
        nullptr,
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_close_count = true;
            EXPECT_FALSE(client.is_open());
            client.schedule_removal();

            // sending data after shutdown -> error
            client.send_data("ololo", [](io::TcpClient& client, const io::Error& error) {
                EXPECT_TRUE(error);
                EXPECT_EQ(io::StatusCode::NOT_CONNECTED, error.code());
            });
        }
    );

    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);
    EXPECT_EQ(0, client_on_close_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(1, server_on_close_count);
    EXPECT_EQ(1, client_on_close_count);
}

TEST_F(TcpClientServerTest, cancel_error_of_sending_server_data_to_client) {
    // Note: in this test server sends large cunk of data to client but in receive callback closes that connection
    //       which does not allow to complete that sending operation, thus it is cancelled

    // Note2: on Windows send consumes all supplied buffer at once. And as we close connection from the client side,
    //        when it is happened entire buffer was consumed and successfull callback returned,
    //        so operation will not be cancelled and result will be OK.
#ifdef WIN32
    IO_TEST_SKIP();
#endif // WIN32

    const std::size_t DATA_SIZE = 64 * 1024 * 1024;

    std::shared_ptr<char> buf(new char[DATA_SIZE], std::default_delete<char[]>());

    for (std::size_t i = 0; i < DATA_SIZE; ++i) {
        buf.get()[i] = static_cast<char>(i);
    }

    std::size_t server_on_send_callback_count = 0;

    io::TcpClient* client = nullptr;

    io::EventLoop loop;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data(buf, DATA_SIZE,
                [&](io::TcpConnectedClient& client, const io::Error& error) {
                    ++server_on_send_callback_count;
                    EXPECT_TRUE(error);
                    EXPECT_EQ(io::StatusCode::OPERATION_CANCELED, error.code());
                }
            );
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            client.close();
        },
        nullptr
    );
    EXPECT_FALSE(listen_error);

    client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data("_",
                [](io::TcpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                }
            );
        },
        [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            server->close([&](io::TcpServer& server, const io::Error& error) {
                EXPECT_FALSE(error);
                server.schedule_removal();
            });
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_on_send_callback_count);

    EXPECT_FALSE(loop.run());

    EXPECT_EQ(1, server_on_send_callback_count);
}

TEST_F(TcpClientServerTest, cancel_error_of_sending_client_data_to_server) {
    // Note: in this test client sends large cunk of data to server but in receive callback closes that connection
    //       which does not allow to complete that sending operation, thus it is cancelled

#ifdef WIN32
    IO_TEST_SKIP();
#endif // WIN32

    const std::size_t DATA_SIZE = 64 * 1024 * 1024;

    std::shared_ptr<char> buf(new char[DATA_SIZE], std::default_delete<char[]>());

    for (std::size_t i = 0; i < DATA_SIZE; ++i) {
        buf.get()[i] = static_cast<char>(i);
    }

    std::size_t client_on_send_callback_count = 0;

    io::TcpClient* client_ptr = nullptr;

    io::EventLoop loop;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data("_",
                [](io::TcpConnectedClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                }
            );
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        },
        [&](io::TcpConnectedClient& /*client*/, const io::Error& error) {
            EXPECT_FALSE(error);
            server->schedule_removal();
            client_ptr->schedule_removal();
        }
    );
    EXPECT_FALSE(listen_error);

    client_ptr = new io::TcpClient(loop);
    client_ptr->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data(buf, DATA_SIZE,
                [&](io::TcpClient& client, const io::Error& error) {
                    ++client_on_send_callback_count;
                    EXPECT_TRUE(error);
                    EXPECT_EQ(io::StatusCode::OPERATION_CANCELED, error.code());
                }
            );
        },
        [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            client.close();
        },
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        }
    );

    EXPECT_EQ(0, client_on_send_callback_count);

    EXPECT_FALSE(loop.run());

    EXPECT_EQ(1, client_on_send_callback_count);
}

TEST_F(TcpClientServerTest, client_schedule_removal_with_send) {
    io::EventLoop loop;

    auto server = new io::TcpServer(loop);
    io::TcpClient* client_ptr = nullptr;

    std::shared_ptr<char> message_ptr(new char[100], std::default_delete<char[]>());
    std::memset(message_ptr.get(), 0, 100);

    int counter = 0;

    std::function<void(io::TcpConnectedClient&)> send_data = [&](io::TcpConnectedClient& client) {
        if (!client.is_open()) {
            return;
        }

        client.send_data(message_ptr, 100,
            [&](io::TcpConnectedClient& client, const io::Error& error) {
                if (error) {
                    // Chance to reproduce is about 5-10%
                    // In this test client closes conenction first and sends RESET to server.
                    // Server may send some data meantime and connection may become reset from client side (data will not be delivered) but not closed yet
                    // And this callback is called with "BROKEN_PIPE" error and than close.
                    // This error means that message was not delivered because peer closed connection.
                    // TODO: BROKEN_PIPE error name is not descriptiove enough. Convert all such errors into CONNECTION_RESET_BY_PEER????
                    // TODO: OPERATION_CANCELED is returned on Windows here. Need to generalize this?
                    EXPECT_TRUE(io::StatusCode::BROKEN_PIPE == error.code() ||
                                io::StatusCode::OPERATION_CANCELED == error.code()) << error.string();
                }

                send_data(client);
            }
        );

        if (counter++ == 5) {
            // Imitation of sudden client exit
            client_ptr->schedule_removal();
        }
    };

    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            send_data(client);
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        },
        [&](io::TcpConnectedClient&, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::CONNECTION_RESET_BY_PEER, error.code());
            server->schedule_removal();
        }
    );

    ASSERT_FALSE(listen_error) << listen_error.string();

    client_ptr = new io::TcpClient(loop);
    client_ptr->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
        }
    );

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TcpClientServerTest, client_send_without_connect_with_callback) {
    io::EventLoop loop;

    auto client = new io::TcpClient(loop);
    client->send_data("Hello",
        [](io::TcpClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::NOT_CONNECTED, error.code());
            client.schedule_removal();
        }
    );

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TcpClientServerTest, client_send_without_connect_no_callback) {
    io::EventLoop loop;

    auto client = new io::TcpClient(loop);
    client->send_data("Hello"); // Just do nothing and hope for miracle
    client->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TcpClientServerTest, client_saves_received_buffer) {
    io::EventLoop loop;

    const std::string client_message = "Hello!";
    const std::string server_message_1 = "abcdefghijklmnopqrstuvwxyz";
    const std::string server_message_2 = "0123456789";

    std::size_t client_receive_counter = 0;
    std::size_t server_receive_counter = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            //client.delay_send(false); // disabling Nagle's algorithm
            EXPECT_FALSE(error);
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            if (server_receive_counter == 0) {
                client.send_data(server_message_1,
                    [&](io::TcpConnectedClient& client, const io::Error& error) {
                        EXPECT_FALSE(error);
                    }
                );
            } else {
                client.send_data(server_message_2,
                    [&](io::TcpConnectedClient& client, const io::Error& error) {
                        EXPECT_FALSE(error);
                        client.server().shutdown([&](io::TcpServer& server, const io::Error& error) {
                            EXPECT_FALSE(error);
                            server.schedule_removal();
                        });
                    }
                );
            }

            ++server_receive_counter;
        },
    nullptr);

    ASSERT_FALSE(listen_error);

    io::DataChunk saved_buffer;

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            client.send_data(client_message,
                [&](io::TcpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                }
            );
        },
        [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            if (client_receive_counter == 0) {
                std::string s(data.buf.get(), data.size);
                EXPECT_EQ(server_message_1, s);
                saved_buffer = data;

                client.send_data(client_message,
                    [&](io::TcpClient& client, const io::Error& error) {
                        EXPECT_FALSE(error);
                    }
                );
            } else {
                std::string s_1(saved_buffer.buf.get(), saved_buffer.size);
                EXPECT_EQ(server_message_1, s_1);

                std::string s_2(data.buf.get(), data.size);
                EXPECT_EQ(server_message_2, s_2);

                client.schedule_removal();
            }

            ++client_receive_counter;
        }
    );

    EXPECT_EQ(0, client_receive_counter);
    EXPECT_EQ(0, server_receive_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(2, client_receive_counter);
    EXPECT_EQ(2, server_receive_counter);
}

TEST_F(TcpClientServerTest, reuse_client_connection_after_disconnect_from_server) {
    std::size_t client_receive_counter = 0;
    std::size_t client_close_counter = 0;

    io::EventLoop loop;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data("go away!",
                [&](io::TcpConnectedClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    client.close();
                }
            );
        },
        nullptr,
        nullptr);
    EXPECT_FALSE(listen_error);

    auto client_on_connect = [&](io::TcpClient& client, const io::Error& error) {
        EXPECT_FALSE(error);

        client.send_data("Hello!",
            [&](io::TcpClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
            }
        );
    };

    auto client_on_receive = [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        ++client_receive_counter;
    };

    std::function<void(io::TcpClient& client, const io::Error& error)> client_on_close = nullptr;
    client_on_close = [&](io::TcpClient& client, const io::Error& error) {
        if (error) {
            // TODO: investigate this
            EXPECT_EQ(io::StatusCode::CONNECTION_RESET_BY_PEER, error.code());
        }

        if (client_close_counter == 0) {
            client.connect({m_default_addr, m_default_port},
                client_on_connect,
                client_on_receive,
                client_on_close
            );
        } else {
            client.schedule_removal();
            server->schedule_removal();
        }

        ++client_close_counter;
    };

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        client_on_connect,
        client_on_receive,
        client_on_close
    );

    EXPECT_EQ(0, client_receive_counter);
    EXPECT_EQ(0, client_close_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(2, client_receive_counter);
    EXPECT_EQ(2, client_close_counter);
}

TEST_F(TcpClientServerTest, client_communicate_with_multiple_servers_in_threads) {
    // Note: as messages are very short and distinct here, we do not need to care about message boundaries.
    //       But in general such practice should be avoided.

    std::vector<std::thread> server_threads;

    const std::size_t SERVERS_COUNT = 10;
    const std::string client_hello = "who_you_are";
    const std::string client_ask_next = "next_server";
    const std::string client_ask_shutdown = "shutdown_please";
    const std::string client_final_message = "hooray";
    const std::string server_unknow_message = "unknown";

    const std::string target_server_name = "server_" + std::string(1, 'a' + SERVERS_COUNT - 1);

    for (std::size_t i = 0; i < SERVERS_COUNT; ++i) {
        server_threads.emplace_back([&, i](std::string server_name){
            io::EventLoop loop;

            auto server = new io::TcpServer(loop);
            auto listen_error = server->listen({m_default_addr, std::uint16_t(m_default_port + i)},
                [&](io::TcpConnectedClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                },
                [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
                    EXPECT_FALSE(error);
                    const std::string received_message(data.buf.get(), data.size);

                    if (received_message == client_hello) {
                        client.send_data(server_name);
                    } else if (received_message == client_ask_next) {
                        client.send_data(std::to_string(server->endpoint().port() + 1));
                    } else if (received_message == client_ask_shutdown) {
                        server->schedule_removal();
                    } else {
                        client.send_data(server_unknow_message);
                    }
                },
                nullptr
            );
            ASSERT_FALSE(listen_error);

            ASSERT_EQ(io::StatusCode::OK, loop.run());

        }, std::string("server_") + std::string(1, 'a' + i));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::size_t client_on_connect_count = 0;
    std::size_t client_server_found_count = 0;

    io::EventLoop loop;

    std::function<void(io::TcpClient&, const io::Error&)> client_on_connect;
    std::function<void(io::TcpClient&, const io::DataChunk&, const io::Error&)> client_on_receive;
    std::function<void(io::TcpClient&, const io::Error&)> client_on_close;

    client_on_connect = [&](io::TcpClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
        ++client_on_connect_count;
        client.send_data(client_hello);
    };

    client_on_receive = [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);
        const std::string received_message(data.buf.get(), data.size);

        if (received_message.find("server_") == 0) {
            if (received_message == target_server_name) {
                ++client_server_found_count;
                client.send_data(client_final_message);
            } else {
                client.send_data(client_ask_next);
            }
        } else if (received_message == server_unknow_message) {
            client.send_data(client_ask_shutdown);
            client.close();
        } else {
            auto new_port = static_cast<std::uint16_t>(std::stoi(received_message));
            ASSERT_NE(0, new_port);
            client.send_data(client_ask_shutdown,
                [&, new_port](io::TcpClient& client, const io::Error& error) {
                    client.connect({m_default_addr, new_port},
                        client_on_connect,
                        client_on_receive,
                        client_on_close
                    );
                }
            );
        }
    };

    client_on_close = [&](io::TcpClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
        client.schedule_removal();
    };

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        client_on_connect,
        client_on_receive,
        client_on_close
    );

    EXPECT_EQ(0, client_server_found_count);
    EXPECT_EQ(0, client_on_connect_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_server_found_count);
    EXPECT_EQ(SERVERS_COUNT, client_on_connect_count);

    for (auto& t : server_threads) {
        t.join();
    }
}

TEST_F(TcpClientServerTest, connect_to_other_server_in_connect_callback) {
    io::EventLoop loop;

    std::size_t server_1_on_new_client_count = 0;
    std::size_t server_2_on_new_client_count = 0;
    std::size_t server_3_on_new_client_count = 0;

    std::size_t client_on_connect_ok_count = 0;
    std::size_t client_on_connect_fail_count = 0;

    auto server_on_new_client = [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error);
        *reinterpret_cast<std::size_t*>(client.server().user_data()) += 1;
        client.server().schedule_removal();
    };

    auto server_1 = new io::TcpServer(loop);
    server_1->set_user_data(&server_1_on_new_client_count);
    auto listen_error_1 = server_1->listen(
        {m_default_addr, m_default_port},
        server_on_new_client,
        nullptr,
        nullptr);
    EXPECT_FALSE(listen_error_1);

    auto server_2 = new io::TcpServer(loop);
    server_2->set_user_data(&server_2_on_new_client_count);
    auto listen_error_2 = server_2->listen(
        {m_default_addr, std::uint16_t(m_default_port + 1)},
        server_on_new_client,
        nullptr,
        nullptr);
    EXPECT_FALSE(listen_error_2);

    auto server_3 = new io::TcpServer(loop);
    server_3->set_user_data(&server_3_on_new_client_count);
    auto listen_error_3 = server_3->listen(
        {m_default_addr, std::uint16_t(m_default_port + 2)},
        server_on_new_client,
        nullptr,
        nullptr);
    EXPECT_FALSE(listen_error_3);

    std::uint16_t port = m_default_port;

    std::function<void(io::TcpClient& client, const io::Error& error)> client_on_connect;
    client_on_connect = [&](io::TcpClient& client, const io::Error& error) {
        if (error) {
            ++client_on_connect_fail_count;
        } else {
            ++client_on_connect_ok_count;
        }

        if (client_on_connect_fail_count == 3) {
            client.schedule_removal();
            return;
        }

        ++port;
        client.connect({m_default_addr, port}, client_on_connect, nullptr, nullptr);
    };

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, port}, client_on_connect, nullptr, nullptr);

    EXPECT_EQ(0, client_on_connect_fail_count);
    EXPECT_EQ(0, client_on_connect_ok_count);

    EXPECT_EQ(0, server_1_on_new_client_count);
    EXPECT_EQ(0, server_2_on_new_client_count);
    EXPECT_EQ(0, server_3_on_new_client_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(3, client_on_connect_fail_count);
    EXPECT_EQ(3, client_on_connect_ok_count);

    EXPECT_EQ(1, server_1_on_new_client_count);
    EXPECT_EQ(1, server_2_on_new_client_count);
    EXPECT_EQ(1, server_3_on_new_client_count);
}

// TODO: disabled because each platform has different results here
//       currently it is working on LINUX
TEST_F(TcpClientServerTest, DISABLED_connect_to_other_server_in_receive_callback) {
    static const std::size_t SERVER_DATA_SIZE = 64 * 1024 * 1024;

    std::thread server_1_thread([this](){
        io::EventLoop loop;

        std::size_t server_on_new_connection_count = 0;
        std::size_t server_on_send_count = 0;
        std::size_t server_on_close_client_count = 0;

        auto server_on_new_client = [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_new_connection_count;

            std::shared_ptr<char> buf(new char[SERVER_DATA_SIZE], std::default_delete<char[]>());
            std::memset(buf.get(), 0, SERVER_DATA_SIZE);
            client.send_data(buf, SERVER_DATA_SIZE,
                [&](io::TcpConnectedClient& client, const io::Error& error) {
                    EXPECT_EQ(0, server_on_close_client_count);
                    ++server_on_send_count;
                    EXPECT_TRUE(error) << error.string();
                    EXPECT_EQ(io::StatusCode::OPERATION_CANCELED, error.code());
                    client.server().schedule_removal();
                }
            );
        };

        auto server_on_client_close = [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::CONNECTION_RESET_BY_PEER, error.code());
            ++server_on_close_client_count;
            EXPECT_EQ(1, client.pending_write_requesets());
        };

        auto server = new io::TcpServer(loop);
        auto listen_error = server->listen(
            {m_default_addr, m_default_port},
            server_on_new_client,
            nullptr,
            server_on_client_close);
        EXPECT_FALSE(listen_error);

        EXPECT_EQ(0, server_on_new_connection_count);
        EXPECT_EQ(0, server_on_send_count);
        EXPECT_EQ(0, server_on_close_client_count);

        ASSERT_EQ(io::StatusCode::OK, loop.run());

        EXPECT_EQ(1, server_on_new_connection_count);
        EXPECT_EQ(1, server_on_send_count);
        EXPECT_EQ(1, server_on_close_client_count);
    });

    std::thread server_2_thread([this](){
        io::EventLoop loop;

        std::size_t server_on_new_connection_count = 0;
        std::size_t server_on_send_count = 0;
        std::size_t server_on_close_client_count = 0;

        auto server_on_new_client = [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_new_connection_count;

            std::shared_ptr<char> buf(new char[SERVER_DATA_SIZE], std::default_delete<char[]>());
            std::memset(buf.get(), 1, SERVER_DATA_SIZE);
            client.send_data(buf, SERVER_DATA_SIZE,
                [&](io::TcpConnectedClient& client, const io::Error& error) {
                    ++server_on_send_count;
                    EXPECT_FALSE(error) << error.string();
                    client.server().schedule_removal();
                }
            );
        };

        auto server_on_client_close = [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_close_client_count;
            EXPECT_EQ(0, client.pending_write_requesets());
        };

        auto server = new io::TcpServer(loop);
        auto listen_error = server->listen(
            {m_default_addr, std::uint16_t(m_default_port + 1)},
            server_on_new_client,
            nullptr,
            server_on_client_close);
        EXPECT_FALSE(listen_error);

        EXPECT_EQ(0, server_on_new_connection_count);
        EXPECT_EQ(0, server_on_send_count);
        EXPECT_EQ(0, server_on_close_client_count);

        ASSERT_EQ(io::StatusCode::OK, loop.run());

        EXPECT_EQ(1, server_on_new_connection_count);
        EXPECT_EQ(1, server_on_send_count);
        EXPECT_EQ(1, server_on_close_client_count);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    io::EventLoop loop;
    std::size_t client_on_new_connection_count = 0;

    std::uint16_t port = m_default_port;

    std::function<void(io::TcpClient& client, const io::Error& error)> client_on_connect;
    std::function<void(io::TcpClient& client, const io::DataChunk&, const io::Error& error)> client_on_receive;
    std::function<void(io::TcpClient& client, const io::Error& error)> client_on_close;

    client_on_connect = [&](io::TcpClient& client, const io::Error& error) {
        ++client_on_new_connection_count;
    };

    client_on_receive = [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
        EXPECT_FALSE(error);

        if (client_on_new_connection_count == 1) {
            client.connect({m_default_addr, ++port}, client_on_connect, client_on_receive, client_on_close);
            EXPECT_EQ(0, data.buf.get()[0]);
        } else {
            EXPECT_EQ(1, data.buf.get()[0]);
        }
    };

    client_on_close = [&](io::TcpClient& client, const io::Error& error) {
        if (client_on_new_connection_count == 2) {
            client.schedule_removal();
            return;
        }
    };

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, port}, client_on_connect, client_on_receive, client_on_close);

    EXPECT_EQ(0, client_on_new_connection_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    server_1_thread.join();
    server_2_thread.join();

    EXPECT_EQ(2, client_on_new_connection_count);
}

TEST_F(TcpClientServerTest, server_send_lot_small_chunks_to_many_connected_clients) {
    const std::size_t CLIENTS_COUNT = 40;
    const std::size_t DATA_TO_SEND_SIZE = 20 * 1024;

    using UnderlyingType = std::uint64_t;
    static const std::size_t UNDERLYING_TYPE_SIZE = sizeof(UnderlyingType);

    static_assert(DATA_TO_SEND_SIZE % UNDERLYING_TYPE_SIZE == 0, "Data should be aligned");

    std::shared_ptr<char> buffers[CLIENTS_COUNT];
    for (std::size_t i = 0; i < CLIENTS_COUNT; ++i) {
        buffers[i].reset(new char[DATA_TO_SEND_SIZE], std::default_delete<char[]>());

        for (std::size_t k = 0; k < DATA_TO_SEND_SIZE / UNDERLYING_TYPE_SIZE; ++k) {
            *reinterpret_cast<UnderlyingType*>(&buffers[i].get()[k * UNDERLYING_TYPE_SIZE]) = i * DATA_TO_SEND_SIZE + k;
        }
    }

    std::thread server_thread([&](){
        io::EventLoop loop;

        std::size_t server_on_connect_count = 0;
        std::size_t server_on_client_count = 0;

        struct ClientData {
            ClientData(std::size_t id_) :
                id(id_),
                offset(0),
                buf(new char[UNDERLYING_TYPE_SIZE], std::default_delete<char[]>()) {
            }

            std::size_t id;
            std::size_t offset;
            std::shared_ptr<char> buf;
        };

        std::function<void(io::TcpConnectedClient&, const io::Error&)> on_send =
            [&](io::TcpConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                auto client_data = reinterpret_cast<ClientData*>(client.user_data());

                if (client_data->offset == DATA_TO_SEND_SIZE) {
                    delete client_data;
                    client.shutdown();
                    return;
                }

                std::memcpy(client_data->buf.get(), buffers[client_data->id].get() + client_data->offset, UNDERLYING_TYPE_SIZE);
                client.send_data(client_data->buf, UNDERLYING_TYPE_SIZE, on_send);

                client_data->offset += UNDERLYING_TYPE_SIZE;
            };

        auto server = new io::TcpServer(loop);
        auto listen_error = server->listen(
            {m_default_addr, m_default_port},
            [&](io::TcpConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                ++server_on_connect_count;
            },
            [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
                EXPECT_FALSE(error);

                const std::string received_message(data.buf.get(), data.size);

                std::size_t client_id = std::stoul(received_message);
                EXPECT_LT(client_id, CLIENTS_COUNT);
                auto client_data = new ClientData(client_id);
                client.set_user_data(client_data);

                std::memcpy(client_data->buf.get(), buffers[client_id].get(), UNDERLYING_TYPE_SIZE);
                client_data->offset += UNDERLYING_TYPE_SIZE;

                client.send_data(client_data->buf, UNDERLYING_TYPE_SIZE, on_send);
            },
            [&](io::TcpConnectedClient&, const io::Error&) {
                ++server_on_client_count;
                if (server_on_client_count == CLIENTS_COUNT) {
                    server->schedule_removal();
                }
            }
        );
        ASSERT_FALSE(listen_error);

        EXPECT_EQ(0, server_on_connect_count);

        ASSERT_EQ(io::StatusCode::OK, loop.run());

        EXPECT_EQ(CLIENTS_COUNT, server_on_connect_count);
    });

    std::vector<std::thread> client_threads;

    for (std::size_t i = 0; i < CLIENTS_COUNT; ++i) {
        client_threads.emplace_back([&](std::size_t thread_id) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            std::size_t client_on_data_receive_bytes_count = 0;

            io::EventLoop loop;
            auto client = new io::TcpClient(loop);
            client->connect(
                {m_default_addr, m_default_port},
                [&](io::TcpClient& client, const io::Error& error){
                    EXPECT_FALSE(error) << error.string();
                    client.send_data(std::to_string(thread_id));
                },
                [&](io::TcpClient& client, const io::DataChunk& data, const io::Error&){
                    const std::size_t start_offset = client_on_data_receive_bytes_count;
                    client_on_data_receive_bytes_count += data.size;

                    for (auto i = start_offset; i < client_on_data_receive_bytes_count; i += UNDERLYING_TYPE_SIZE) {
                        ASSERT_EQ(thread_id * DATA_TO_SEND_SIZE + i / UNDERLYING_TYPE_SIZE,
                                  *reinterpret_cast<const UnderlyingType*>(data.buf.get() + (i - start_offset)));
                    }
                },
                [&](io::TcpClient& client, const io::Error& error){
                    EXPECT_FALSE(error) << error.string();
                    client.schedule_removal();
                }
            );

            EXPECT_EQ(0, client_on_data_receive_bytes_count);

            ASSERT_EQ(io::StatusCode::OK, loop.run());

            EXPECT_EQ(DATA_TO_SEND_SIZE, client_on_data_receive_bytes_count);
        },
        i);
    }

    for (std::size_t i = 0; i < CLIENTS_COUNT; ++i) {
        client_threads[i].join();
    }

    server_thread.join();
}

TEST_F(TcpClientServerTest, send_data_of_size_0) {
    io::EventLoop loop;

    std::size_t server_on_send_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t client_on_send_count = 0;
    std::size_t client_on_receive_count = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
    [&](io::TcpConnectedClient& client, const io::Error& error) {
        EXPECT_FALSE(error);

        std::shared_ptr<char> buf(new char[4], std::default_delete<char[]>());

        client.send_data(buf, 0); // silent fail

        client.send_data(buf, 0,
            [&](io::TcpConnectedClient& client, const io::Error& error) {
                ++server_on_send_count;
                EXPECT_TRUE(error);
                EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
                server->schedule_removal();
            }
        );
    },
    [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
        ++server_on_receive_count;
    },
    nullptr);
    ASSERT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
    [&](io::TcpClient& client, const io::Error& error) {
        EXPECT_FALSE(error);

        client.send_data("",
            [&](io::TcpClient& client, const io::Error& error) {
                ++client_on_send_count;
                EXPECT_TRUE(error);
                EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
                client.schedule_removal();
            }
        );
    },
    [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
        ++client_on_receive_count;
        EXPECT_FALSE(error);
    });

    EXPECT_EQ(0, server_on_send_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, client_on_send_count);
    EXPECT_EQ(0, client_on_receive_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_send_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(1, client_on_send_count);
    EXPECT_EQ(0, client_on_receive_count);
}

TEST_F(TcpClientServerTest, connected_client_write_after_close_in_server_receive_callback) {
    io::EventLoop loop;

    std::size_t server_on_send_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t client_on_receive_count = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({"0.0.0.0", m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_count;
            client.close();
            client.send_data("!!!",
                [&](io::TcpConnectedClient& client, const io::Error& error) {
                    EXPECT_TRUE(error);
                    ++server_on_send_count;
                    EXPECT_EQ(io::StatusCode::NOT_CONNECTED, error.code());
                }
            );
        },
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            client.server().schedule_removal();
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);

            client.send_data("!",
                [&](io::TcpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                }
            );
        },
        [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_receive_count;
        },
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_on_send_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, client_on_receive_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_send_count);
    EXPECT_EQ(1, server_on_receive_count);
    EXPECT_EQ(0, client_on_receive_count);
}

TEST_F(TcpClientServerTest, client_schedule_removal_during_large_chunk_send) {
    io::EventLoop loop;

    std::size_t server_on_close_count = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen(
        {m_default_addr, m_default_port},
        nullptr,
        nullptr,
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_close_count;
            server->schedule_removal();
        }
    );
    EXPECT_FALSE(listen_error);

    const std::size_t DATA_SIZE = 64 * 1024 * 1024;
    std::shared_ptr<char> buf(new char[DATA_SIZE], std::default_delete<char[]>());
    for (std::size_t i = 0; i < DATA_SIZE; ++i) {
        buf.get()[i] = static_cast<char>(i);
    }

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data(buf, DATA_SIZE);
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_on_close_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_close_count);
}

TEST_F(TcpClientServerTest, server_schedule_removal_during_large_chunk_send) {
    io::EventLoop loop;

    std::size_t client_on_close_count = 0;

    const std::size_t DATA_SIZE = 16 * 1024 * 1024;
    std::shared_ptr<char> buf(new char[DATA_SIZE], std::default_delete<char[]>());
    for (std::size_t i = 0; i < DATA_SIZE; ++i) {
        buf.get()[i] = static_cast<char>(i);
    }

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen(
        {m_default_addr, m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            client.send_data(buf, DATA_SIZE);
            server->schedule_removal();
        },
        nullptr,
        nullptr
    );
    EXPECT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        nullptr,
        nullptr,
        [&](io::TcpClient& client, const io::Error& error) {
            // Here is no CONNECTION_RESET_BY_PEER error because sending is closing connection,
            // so it is expected.
            EXPECT_FALSE(error);
            client.schedule_removal();
            ++client_on_close_count;
        }
    );

    EXPECT_EQ(0, client_on_close_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_close_count);
}

void TcpClientServerTest::test_impl_client_double_close(
    std::function<void(io::TcpClient& client)> close_variant) {

    io::EventLoop loop;

    std::size_t client_on_close_count = 0;
    std::size_t server_on_close_count = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen(
        {m_default_addr, m_default_port},
        nullptr,
        nullptr,
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_close_count;
            server->schedule_removal();
        }
    );
    EXPECT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            close_variant(client);

            auto timer = new io::Timer(loop);
            timer->start(200,
                [&](io::Timer& timer){
                    timer.schedule_removal();
                    client.schedule_removal();
                }
            );
        },
        nullptr,
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            close_variant(client); // Note: close inside close callback is NOP
            ++client_on_close_count;
        }
    );

    EXPECT_EQ(0, client_on_close_count);
    EXPECT_EQ(0, server_on_close_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_close_count);
    EXPECT_EQ(1, server_on_close_count);
}

TEST_F(TcpClientServerTest, client_double_close) {
    test_impl_client_double_close(
        [](io::TcpClient& client) {
            client.close();
        }
    );
}

TEST_F(TcpClientServerTest, client_double_shutdown) {
    test_impl_client_double_close(
        [](io::TcpClient& client) {
            client.shutdown();
        }
    );
}

void TcpClientServerTest::test_impl_server_double_close(
    std::function<void(io::TcpServer& server,
                       std::function<void(io::TcpServer&, const io::Error& error)> callback)> close_variant) {

    io::EventLoop loop;

    std::size_t client_on_close_count = 0;
    std::size_t server_on_close_count = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen(
        {m_default_addr, m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            close_variant(*server, [&](io::TcpServer&, const io::Error& error) {
                ++server_on_close_count;
                EXPECT_FALSE(error);
                close_variant(*server, [&](io::TcpServer& server, const io::Error& error) {
                    EXPECT_TRUE(error);
                    ++server_on_close_count;
                    server.schedule_removal();
                });
            });
        },
        nullptr,
        nullptr
    );
    EXPECT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        nullptr,
        nullptr,
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_close_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_on_close_count);
    EXPECT_EQ(0, server_on_close_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_close_count);
    EXPECT_EQ(2, server_on_close_count);
}

TEST_F(TcpClientServerTest, server_double_close) {
    test_impl_server_double_close(
        [&](io::TcpServer& server,
            std::function<void(io::TcpServer&, const io::Error& error)> callback) {
            server.close(callback);
        }
    );
}

TEST_F(TcpClientServerTest, server_double_shutdown) {
    test_impl_server_double_close(
        [&](io::TcpServer& server,
            std::function<void(io::TcpServer&, const io::Error& error)> callback) {
            server.shutdown(callback);
        }
    );
}

TEST_F(TcpClientServerTest, client_shutdown_not_connected_1) {
    io::EventLoop loop;

    auto client = new io::TcpClient(loop);
    client->shutdown();
    client->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TcpClientServerTest, client_shutdown_not_connected_2) {
    io::EventLoop loop;

    std::size_t client_on_close_count = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen(
        {m_default_addr, m_default_port},
        nullptr,
        nullptr,
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            server->schedule_removal();
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.shutdown();
        },
        nullptr,
        [&](io::TcpClient& client, const io::Error& error) {
            ++client_on_close_count;
            EXPECT_FALSE(error);
            client.shutdown();
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_on_close_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, client_on_close_count);
}

TEST_F(TcpClientServerTest, server_shutdown_not_listening_1) {
    io::EventLoop loop;

    std::size_t server_on_close_count = 0;

    auto server = new io::TcpServer(loop);
    server->shutdown();
    server->shutdown([&](io::TcpServer&, const io::Error& error) {
        EXPECT_TRUE(error);
        ++server_on_close_count;
        // DOC: schedule_removal should be last command related to this object or this callback
        server->schedule_removal();
    });

    EXPECT_EQ(0, server_on_close_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_close_count);
}

TEST_F(TcpClientServerTest, server_shutdown_not_listening_2) {
    io::EventLoop loop;

    std::size_t server_on_shutdown_1_count = 0;
    std::size_t server_on_shutdown_2_count = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen(
        {m_default_addr, m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            server->shutdown([&](io::TcpServer&, const io::Error& error) {
                ++server_on_shutdown_1_count;
                EXPECT_FALSE(error);
                server->shutdown([&](io::TcpServer&, const io::Error& error) {
                    ++server_on_shutdown_2_count;
                    EXPECT_TRUE(error);
                    server->schedule_removal();
                });
            });
        },
        nullptr,
        nullptr
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::TcpClient(loop);
    client->connect({m_default_addr, m_default_port},
        nullptr,
        nullptr,
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_on_shutdown_1_count);
    EXPECT_EQ(0, server_on_shutdown_2_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, server_on_shutdown_1_count);
    EXPECT_EQ(1, server_on_shutdown_2_count);
}

TEST_F(TcpClientServerTest, client_schedule_removal_in_on_send_callback) {
    // Note: in this test we check that server is able to receive all the data
    //       if removal is scheduled in on_send callback
    // DOC: successfull on_send callback does not mean that data was received by other side. It only means that data was
    //      successfully transfered to OS protocol factilities.

    const std::size_t DATA_SIZE = 32 * 1024 * 1024;

    std::thread server_thread([&]{
        io::EventLoop loop;

        std::size_t server_received_size = 0;

        auto server = new io::TcpServer(loop);
        auto listen_error = server->listen({m_default_addr, m_default_port},
            [&](io::TcpConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
            },
            [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
                EXPECT_FALSE(error);
                server_received_size += data.size;
            },
            [&](io::TcpConnectedClient& /*client*/, const io::Error& error) {
                EXPECT_FALSE(error);
                server->schedule_removal();
            }
        );
        EXPECT_FALSE(listen_error);

        EXPECT_EQ(0, server_received_size);

        EXPECT_FALSE(loop.run());

        EXPECT_EQ(DATA_SIZE, server_received_size);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::shared_ptr<char> buf(new char[DATA_SIZE], std::default_delete<char[]>());
    std::memset(buf.get(), 0, DATA_SIZE);

    std::size_t client_on_send_counter = 0;

    io::EventLoop loop;

    auto client_ptr = new io::TcpClient(loop);
    client_ptr->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.send_data(buf, DATA_SIZE,
                [&](io::TcpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    ++client_on_send_counter;
                    client.schedule_removal();
                }
            );
        },
        nullptr,
        nullptr
    );

    EXPECT_EQ(0, client_on_send_counter);

    EXPECT_FALSE(loop.run());
    server_thread.join();

    EXPECT_EQ(1, client_on_send_counter);
}

TEST_F(TcpClientServerTest, server_schedule_removal_in_on_send_callback) {
    const std::size_t DATA_SIZE = 32 * 1024 * 1024;

    std::thread server_thread([&]{
        io::EventLoop loop;

        std::size_t server_on_send_counter = 0;

        std::shared_ptr<char> buf(new char[DATA_SIZE], std::default_delete<char[]>());
        std::memset(buf.get(), 0, DATA_SIZE);

        auto server = new io::TcpServer(loop);
        auto listen_error = server->listen({m_default_addr, m_default_port},
            [&](io::TcpConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                client.send_data(buf, DATA_SIZE,
                    [&](io::TcpConnectedClient& client, const io::Error& error) {
                        EXPECT_FALSE(error);
                        ++server_on_send_counter;
                        client.server().schedule_removal();
                    }
                );
            },
            [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
                EXPECT_FALSE(error);
            },
            [&](io::TcpConnectedClient& /*client*/, const io::Error& error) {
                EXPECT_FALSE(error);
            }
        );
        EXPECT_FALSE(listen_error);

        EXPECT_EQ(0, server_on_send_counter);

        EXPECT_FALSE(loop.run());

        EXPECT_EQ(1, server_on_send_counter);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    io::EventLoop loop;

    std::size_t client_received_size = 0;

    auto client_ptr = new io::TcpClient(loop);
    client_ptr->connect({m_default_addr, m_default_port},
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            client_received_size += data.size;
        },
        [&](io::TcpClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_received_size);

    EXPECT_FALSE(loop.run());
    server_thread.join();

    EXPECT_EQ(DATA_SIZE, client_received_size);
}

TEST_F(TcpClientServerTest, multiple_connect_calls_to_server) {
    io::EventLoop loop;

    const std::size_t CLIENTS_COUNT = 128;

    std::size_t server_on_connect_counter = 0;
    std::size_t server_on_close_counter = 0;

    auto server = new io::TcpServer(loop);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::TcpConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_connect_counter;
        },
        [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            client.close();
        },
        [&](io::TcpConnectedClient& /*client*/, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_close_counter;
            if (server_on_close_counter == CLIENTS_COUNT) {
                server->schedule_removal();
            }
        },
        CLIENTS_COUNT
    );
    EXPECT_FALSE(listen_error);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<std::thread> client_threads;
    for (std::size_t i = 0; i < CLIENTS_COUNT; ++i) {
        client_threads.emplace_back([this](std::size_t index){
            io::EventLoop loop;

            std::size_t client_on_connect_count = 0;
            std::size_t client_on_read_count = 0;
            std::size_t client_on_close_count = 0;

            auto client_ptr = new io::TcpClient(loop);
            client_ptr->connect({m_default_addr, m_default_port},
                [&](io::TcpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string() << ", index: " << index;
                    ++client_on_connect_count;
                    client.send_data("close");
                },
                [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string() << ", index: " << index;
                    ++client_on_read_count;
                },
                [&](io::TcpClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string() << ", index: " << index;
                    ++client_on_close_count;
                    client.schedule_removal();
                }
            );

            EXPECT_EQ(0, client_on_connect_count);
            EXPECT_EQ(0, client_on_read_count);
            EXPECT_EQ(0, client_on_close_count);

            EXPECT_FALSE(loop.run());

            EXPECT_EQ(1, client_on_connect_count) << "index: " << index;
            EXPECT_EQ(0, client_on_read_count) << "index: " << index;
            EXPECT_EQ(1, client_on_close_count) << "index: " << index;
        },
        i);
    };

    EXPECT_EQ(0, server_on_connect_counter);
    EXPECT_EQ(0, server_on_close_counter);

    EXPECT_FALSE(loop.run());

    std::for_each(client_threads.begin(), client_threads.end(), [](std::thread& t){t.join();});

    EXPECT_EQ(CLIENTS_COUNT, server_on_connect_counter);
    EXPECT_EQ(CLIENTS_COUNT, server_on_close_counter);
}

TEST_F(TcpClientServerTest, client_and_server_simultaneously_send_data_each_other) {
    const std::size_t SERVER_SEND_DATA_SIZE = 64 * 1000 * 1000;
    const std::size_t CLIENT_SEND_DATA_SIZE = 64 * 1024 * 1024;

    static_assert(SERVER_SEND_DATA_SIZE % sizeof(int) == 0, "");
    static_assert(CLIENT_SEND_DATA_SIZE % sizeof(int) == 0, "");

    std::shared_ptr<char> server_send_buf(new char[SERVER_SEND_DATA_SIZE], std::default_delete<char[]>());
    for(std::size_t i = 0; i < SERVER_SEND_DATA_SIZE; i += sizeof(int)) {
        *reinterpret_cast<int*>(server_send_buf.get() + i) = rand();
    }

    std::shared_ptr<char> client_send_buf(new char[CLIENT_SEND_DATA_SIZE], std::default_delete<char[]>());
    for(std::size_t i = 0; i < CLIENT_SEND_DATA_SIZE; i += sizeof(int)) {
        *reinterpret_cast<int*>(client_send_buf.get() + i) = rand();
    }

    std::thread server_thread([&](){
        std::shared_ptr<char> receive_buf(new char[CLIENT_SEND_DATA_SIZE], std::default_delete<char[]>());
        std::memset(receive_buf.get(), 0, CLIENT_SEND_DATA_SIZE);

        bool data_sent = false;
        bool data_received = false;

        auto close_if_required = [&](io::TcpConnectedClient& client) {
            if (data_sent && data_received) {
                client.close();
            }
        };

        io::EventLoop loop;

        auto server = new io::TcpServer(loop);
        auto listen_error = server->listen({m_default_addr, m_default_port},
            [&](io::TcpConnectedClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                client.send_data(server_send_buf, SERVER_SEND_DATA_SIZE,
                    [&](io::TcpConnectedClient& client, const io::Error& error) {
                        data_sent = true;
                        close_if_required(client);
                    }
                );
            },
            [&](io::TcpConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
                EXPECT_FALSE(error);
                ASSERT_LE(data.offset + data.size, CLIENT_SEND_DATA_SIZE);
                if (data.offset + data.size == CLIENT_SEND_DATA_SIZE) {
                    data_received = true;
                    close_if_required(client);
                };

                EXPECT_EQ((client_send_buf.get() + data.offset)[0], data.buf.get()[0]);

                memcpy(receive_buf.get() + data.offset, data.buf.get(), data.size);
            },
            [&](io::TcpConnectedClient&, const io::Error& error) {
                EXPECT_FALSE(error);
                server->schedule_removal();
            }
        );
        EXPECT_FALSE(listen_error);

        ASSERT_FALSE(loop.run());

        EXPECT_EQ(0, std::memcmp(client_send_buf.get(), receive_buf.get(), CLIENT_SEND_DATA_SIZE));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::thread client_thread([&](){
        std::shared_ptr<char> receive_buf(new char[SERVER_SEND_DATA_SIZE], std::default_delete<char[]>());
        std::memset(receive_buf.get(), 0, SERVER_SEND_DATA_SIZE);

        bool data_sent = false;
        bool data_received = false;

        auto close_if_required = [&](io::TcpClient& client) {
            if (data_sent && data_received) {
                client.close();
            }
        };

        io::EventLoop loop;

        auto client_ptr = new io::TcpClient(loop);
        client_ptr->connect({m_default_addr, m_default_port},
            [&](io::TcpClient& client, const io::Error& error) {
                EXPECT_FALSE(error) << error.string();
                client.send_data(client_send_buf, CLIENT_SEND_DATA_SIZE,
                    [&](io::TcpClient& client, const io::Error& error) {
                        data_sent = true;
                        close_if_required(client);
                    }
                );
            },
            [&](io::TcpClient& client, const io::DataChunk& data, const io::Error& error) {
                EXPECT_FALSE(error) << error.string();
                ASSERT_LE(data.offset + data.size, SERVER_SEND_DATA_SIZE);
                if (data.offset + data.size == SERVER_SEND_DATA_SIZE) {
                    data_received = true;
                    close_if_required(client);
                };

                memcpy(receive_buf.get() + data.offset, data.buf.get(), data.size);
            },
            [&](io::TcpClient& client, const io::Error& error) {
                EXPECT_FALSE(error) << error.string();
                client.schedule_removal();
            }
        );

        ASSERT_FALSE(loop.run());

        EXPECT_EQ(0, std::memcmp(server_send_buf.get(), receive_buf.get(), SERVER_SEND_DATA_SIZE));
    });

    server_thread.join();
    client_thread.join();
}

// TODO: Get backlog size on different platforms???
// http://veithen.io/2014/01/01/how-tcp-backlog-works-in-linux.html
// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/listen.2.html


// TODO: ipv6

// TODO: test schedule of server removal when have no connections and try connect many clients right after removal is scheduled
// TODO: send large chunk of bytes and close connection right after close from sending side, ensure that client received not all data

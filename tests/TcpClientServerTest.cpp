#include "UTCommon.h"

#include "io/TcpClient.h"
#include "io/TcpServer.h"
#include "io/ScopeExitGuard.h"
#include "io/Timer.h"

#include <cstdint>
#include <thread>
#include <vector>
#include <memory>

struct TcpClientServerTest : public testing::Test,
                             public LogRedirector {

protected:
    std::uint16_t m_default_port = 31540;
    std::string m_default_addr = "127.0.0.1";
};

TEST_F(TcpClientServerTest, server_default_constructor) {
    io::EventLoop loop;
    io::TcpServer server(loop);

    ASSERT_EQ(0, loop.run());
}

TEST_F(TcpClientServerTest, client_default_constructor) {
    io::EventLoop loop;
    auto client = new io::TcpClient(loop);

    ASSERT_EQ(0, loop.run());
    client->schedule_removal();
}

TEST_F(TcpClientServerTest, 1_client_sends_data_to_server) {
    io::EventLoop loop;

    const std::string message = "Hello world!";

    bool data_received = false;

    io::TcpServer server(loop);
    ASSERT_EQ(0, server.bind("0.0.0.0", m_default_port));
    auto listen_result = server.listen([&](io::TcpServer& server, io::TcpClient& client) -> bool {
        return true;
    },
    [&](io::TcpServer& server, io::TcpClient& client, const char* buf, size_t size) {
        data_received = true;
        std::string received_message(buf, size);

        EXPECT_EQ(size, message.length());
        EXPECT_EQ(received_message, message);
        server.shutdown();
    });

    ASSERT_EQ(0, listen_result);

    bool data_sent = false;

    auto client = new io::TcpClient(loop);
    client->connect(m_default_addr,m_default_port, [&](io::TcpClient& client, const io::Status& status) {
        EXPECT_TRUE(status.ok());

        client.send_data(message, [&](io::TcpClient& client) {
            data_sent = true;
            client.schedule_removal();
        });
    },
    nullptr);

    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(data_sent);
    EXPECT_TRUE(data_received);
}

TEST_F(TcpClientServerTest, 2_clients_send_data_to_server) {
    io::EventLoop loop;

    const std::string message = "Hello world!";

    bool data_received_1 = false;
    bool data_received_2 = false;

    io::TcpServer server(loop);
    ASSERT_EQ(0, server.bind("0.0.0.0", m_default_port));
    server.listen([&](io::TcpServer& server, io::TcpClient& client) -> bool {
        return true;
    },
    [&](io::TcpServer& server, io::TcpClient& client, const char* buf, size_t size) {
        EXPECT_EQ(size, message.length() + 1);

        std::string received_message(buf, size);

        if (message + "1" == received_message) {
            data_received_1 = true;
        } else if (message + "2" == received_message) {
            data_received_2 = true;
        }

        if (data_received_1 && data_received_2) {
            server.shutdown();
        }
    });

    bool data_sent_1 = false;

    auto client_1 = new io::TcpClient(loop);
    client_1->connect(m_default_addr, m_default_port, [&](io::TcpClient& client, const io::Status& status) {
        EXPECT_TRUE(status.ok());

        client.send_data(message + "1", [&](io::TcpClient& client) {
            data_sent_1 = true;
            client.schedule_removal();
        });
    },
    nullptr);

    bool data_sent_2 = false;

    auto client_2 = new io::TcpClient(loop);
    client_2->connect(m_default_addr, m_default_port, [&](io::TcpClient& client, const io::Status& status) {
        EXPECT_TRUE(status.ok());

        client.send_data(message + "2", [&](io::TcpClient& client) {
            data_sent_2 = true;
            client.schedule_removal();
        });
    },
    nullptr);

    ASSERT_EQ(0, loop.run());

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
        io::TcpServer server(loop);
        ASSERT_EQ(0, server.bind("0.0.0.0", m_default_port));
        server.listen([&](io::TcpServer& server, io::TcpClient& client) -> bool {
            return true;
        },
        [&](io::TcpServer& server, io::TcpClient& client, const char* buf, size_t size) {
            EXPECT_EQ(size, message.length());
            EXPECT_EQ(std::string(buf, size), message);
            receive_called = true;
            server.shutdown();
        });

        EXPECT_EQ(0, loop.run());
        EXPECT_TRUE(receive_called);
    });

    std::thread client_thread([this, &message, &send_called](){
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        io::EventLoop loop;
        auto client = new io::TcpClient(loop);
        client->connect(m_default_addr, m_default_port, [&](io::TcpClient& client, const io::Status& status) {
            EXPECT_TRUE(status.ok());

            client.send_data(message, [&](io::TcpClient& client) {
                send_called = true;
                client.schedule_removal();
            });
        },
        nullptr);

        EXPECT_EQ(0, loop.run());
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

    io::TcpServer server(loop);
    ASSERT_EQ(0, server.bind("0.0.0.0", m_default_port));
    server.listen([&](io::TcpServer& server, io::TcpClient& client) -> bool {
        client.send_data(message);
        data_sent = true;
        return true;
    },
    [&](io::TcpServer& server, io::TcpClient& client, const char* buf, size_t size) {
        server.shutdown(); // receive 'shutdown' message
    });


    bool data_received = false;

    auto client = new io::TcpClient(loop);
    client->connect(m_default_addr, m_default_port, [&](io::TcpClient& client, const io::Status& status) {
        EXPECT_TRUE(status.ok());
        // do not send data
    },
    [&](io::TcpClient& client, const char* buf, size_t size) {
        EXPECT_EQ(size, message.length());
        EXPECT_EQ(std::string(buf, size), message);
        data_received = true;

        client.send_data("shutdown", [&](io::TcpClient& client) {
            client.schedule_removal();
        });
    });

    ASSERT_EQ(0, loop.run());
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
        io::TcpServer server(loop);

        std::size_t bytes_received = 0;

        std::vector<unsigned char> total_bytes_received;

        ASSERT_EQ(0, server.bind("0.0.0.0", m_default_port));
        server.listen([&](io::TcpServer& server, io::TcpClient& client) -> bool {
            return true;
        },
        [&](io::TcpServer& server, io::TcpClient& client, const char* buf, size_t size) {
            total_bytes_received.insert(total_bytes_received.end(), buf, buf + size);
            bytes_received += size;
            if (bytes_received >= TOTAL_BYTES) {
                server.shutdown();
            }
        });

        EXPECT_EQ(0, loop.run());
        EXPECT_EQ(TOTAL_BYTES, bytes_received);
        EXPECT_EQ(TOTAL_BYTES, total_bytes_received.size());
        for (std::size_t i = 0; i < total_bytes_received.size(); i += 4) {
            ASSERT_EQ(i / 4, *reinterpret_cast<std::uint32_t*>((&total_bytes_received[0] + i)));
        }
    });

    std::thread client_thread([this, CHUNK_SIZE, CHUNKS_COUNT]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::size_t counter = 0;
        std::function<void(io::TcpClient&)> on_data_sent = [&](io::TcpClient& client) {
            if (++counter == CHUNKS_COUNT) {
                client.schedule_removal();
            }
        };

        io::EventLoop loop;
        auto client = new io::TcpClient(loop);
        client->connect(m_default_addr, m_default_port, [&](io::TcpClient& client, const io::Status& status) {
            EXPECT_TRUE(status.ok());

            for (size_t i = 0; i < CHUNKS_COUNT; ++i) {
                std::shared_ptr<char> buf(new char[CHUNK_SIZE], [](const char* p) { delete[] p; });
                for (std::size_t k = 0; k < CHUNK_SIZE; k += 4) {
                    (*reinterpret_cast<std::uint32_t*>(buf.get() + k) ) = k / 4 + i * CHUNK_SIZE / 4;
                }
                client.send_data(buf, CHUNK_SIZE, on_data_sent);
            }
        },
        nullptr);

        EXPECT_EQ(0, loop.run());
    });

    io::ScopeExitGuard guard([&server_thread, &client_thread](){
        client_thread.join();
        server_thread.join();
    });
}

TEST_F(TcpClientServerTest, client_connect_to_nonexistent_server) {
    bool callback_called = false;

    io::EventLoop loop;
    auto client = new io::TcpClient(loop);
    client->connect(m_default_addr, m_default_port, [&](io::TcpClient& client, const io::Status& status) {
        EXPECT_TRUE(status.fail());
        EXPECT_EQ(io::StatusCode::CONNECTION_REFUSED, status.code());
        callback_called = true;

        // TODO: check client.is_open()

        client.schedule_removal();
    },
    nullptr);

    EXPECT_EQ(0, loop.run());
    EXPECT_TRUE(callback_called);
}

TEST_F(TcpClientServerTest, server_disconnect_client_from_new_connection_callback) {
    io::EventLoop loop;
    io::TcpServer server(loop);

    bool server_receive_callback_called = false;
    bool client_receive_callback_called = false;
    bool client_close_callback_called = false;

    const std::string client_message = "Hello :-)";
    const std::string server_message = "Go away!";

    std::vector<unsigned char> total_bytes_received;

    ASSERT_EQ(0, server.bind("0.0.0.0", m_default_port));
    server.listen([&](io::TcpServer& server, io::TcpClient& client) -> bool {
        client.send_data(server_message);
        EXPECT_EQ(0, server.connected_clients_count());
        return false;
    },
    [&](io::TcpServer& server, io::TcpClient& client, const char* buf, size_t size) {
        server_receive_callback_called = true;
    });

    auto client = new io::TcpClient(loop);
    client->connect(m_default_addr, m_default_port, [&](io::TcpClient& client, const io::Status& status) {
        EXPECT_TRUE(status.ok());
        client.send_data(client_message);
    },
    [&](io::TcpClient& client, const char* buf, size_t size) {
        client_receive_callback_called = true;
        EXPECT_EQ(std::string(buf, size), server_message);
    });

    client->set_close_callback([&](io::TcpClient& client, const io::Status& status) {
        // Not checking status here bacause could be connection reset error
        client_close_callback_called = true;
    });

    io::Timer timer(loop);
    timer.start(500, [&](io::Timer& timer) {
        // Disconnect should be called before timer callback
        EXPECT_TRUE(client_close_callback_called);

        client->schedule_removal();
        server.shutdown();
    });

    EXPECT_EQ(0, loop.run());
    EXPECT_FALSE(server_receive_callback_called);
    EXPECT_TRUE(client_receive_callback_called);
    EXPECT_TRUE(client_close_callback_called);
}

TEST_F(TcpClientServerTest, server_disconnect_client_from_data_receive_callback) {
    const std::string client_message = "Disconnect me!";

    io::EventLoop loop;

    io::TcpServer server(loop);
    ASSERT_EQ(0, server.bind(m_default_addr, m_default_port));
    server.listen([&](io::TcpServer& server, io::TcpClient& client) -> bool {
        return true;
    },
    [&](io::TcpServer& server, io::TcpClient& client, const char* buf, std::size_t size) {
        EXPECT_EQ(std::string(buf, size), client_message);
        client.close();
        EXPECT_EQ(0, server.connected_clients_count());
        // TODO: also test with shutdown
        //client.shutdown();
    });

    bool disconnect_called = false;

    auto client = new io::TcpClient(loop);
    client->connect(m_default_addr, m_default_port,
        [&](io::TcpClient& client, const io::Status& status) {
            EXPECT_TRUE(status.ok());
            client.send_data(client_message);
        },
        [](io::TcpClient& client, const char* buf, size_t size) {
        },
        [&](io::TcpClient& client, const io::Status& status) {
            disconnect_called = true;
        });

    io::Timer timer(loop);
    timer.start(500, [&](io::Timer& timer) {
        // Disconnect should be called before timer callback
        EXPECT_TRUE(disconnect_called);

        client->schedule_removal();
        server.shutdown();
    });

    EXPECT_EQ(0, loop.run());
    EXPECT_TRUE(disconnect_called);
}

TEST_F(TcpClientServerTest, connect_and_simultaneous_send_many_participants) {
    io::EventLoop server_loop;

    std::size_t connections_counter = 0;
    std::size_t server_reads_counter = 0;

    const std::size_t NUMBER_OF_CLIENTS = 100;
    std::vector<bool> clinets_data_log(NUMBER_OF_CLIENTS, false);

    io::TcpServer server(server_loop);
    ASSERT_EQ(0, server.bind(m_default_addr, m_default_port));
    server.listen([&connections_counter](io::TcpServer& server, io::TcpClient& client) -> bool {
        ++connections_counter;
        return true;
    },
    [&clinets_data_log, &server_reads_counter](io::TcpServer& server, io::TcpClient& client, const char* buf, std::size_t size) {
        ++server_reads_counter;
        ASSERT_EQ(sizeof(std::size_t), size);
        const auto value = *reinterpret_cast<const std::size_t*>(buf);
        ASSERT_LT(value, clinets_data_log.size());
        clinets_data_log[value] = true;

        if (server_reads_counter == NUMBER_OF_CLIENTS - 1) {
            server.shutdown();
        }
    });

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
                    [](io::TcpClient& client) {
                        client.schedule_removal();
                    });
            }
        };

        for(std::size_t i = 0; i < NUMBER_OF_CLIENTS; ++i) {
            auto tcp_client = new io::TcpClient(clients_loop);
            tcp_client->set_user_data(reinterpret_cast<void*>(i));
            tcp_client->connect(m_default_addr, m_default_port,
                [i, tcp_client, NUMBER_OF_CLIENTS, send_all_clients, &all_clients]
                (io::TcpClient& client, const io::Status& status) {
                    EXPECT_TRUE(status.ok());
                    all_clients.push_back(tcp_client);

                    if (i == NUMBER_OF_CLIENTS - 1) {
                        ASSERT_EQ(NUMBER_OF_CLIENTS, all_clients.size());
                        send_all_clients();
                    }
                },
            nullptr);
        }

        EXPECT_EQ(0, clients_loop.run());
    });

    io::ScopeExitGuard guard([&client_thread](){
        client_thread.join();
    });

    EXPECT_EQ(0, server_loop.run());
    EXPECT_EQ(NUMBER_OF_CLIENTS, connections_counter);
    EXPECT_EQ(NUMBER_OF_CLIENTS, server_reads_counter);

    for(std::size_t i = 0; i < NUMBER_OF_CLIENTS; ++i) {
        ASSERT_TRUE(clinets_data_log[i]) << " i= " << i;
    }
}

TEST_F(TcpClientServerTest, client_disconnects_from_server) {
    io::EventLoop loop;

    io::TcpServer server(loop);
    server.bind(m_default_addr, m_default_port);

    bool client_close_called = false;
    io::TcpClient::CloseCallback on_client_close =
        [&](io::TcpClient& client, const io::Status& status) {
            EXPECT_TRUE(status.ok());
            client_close_called = true;
        };

    io::TcpServer::NewConnectionCallback on_server_new_connection =
        [=](io::TcpServer& server, io::TcpClient& client) ->bool {
            client.set_close_callback(on_client_close);
            return true;
        };

    io::TcpServer::DataReceivedCallback on_server_receive_data =
        [](io::TcpServer& server, io::TcpClient& client, const char* buf, std::size_t size) {

        };

    server.listen(on_server_new_connection, on_server_receive_data);

    io::TcpClient::ConnectCallback connect_callback =
        [](io::TcpClient& client, const io::Status status) {
            client.close();
        };

    auto client = new io::TcpClient(loop);
    client->connect(m_default_addr, m_default_port, connect_callback, nullptr);

    io::Timer timer(loop);
    timer.start(500, [&](io::Timer& timer) {
        // Disconnect should be called before timer callback
        EXPECT_TRUE(client_close_called);

        client->schedule_removal();
        server.shutdown();
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(client_close_called);
}

// TODO: server sends lot of data to many connected clients

// TODO: client's write after close in server receive callback
// TODO: write large chunks of data and schedule removal
// TODO: double shutdown test
// TODO: shutdown not connected test
// TODO: send-receive large ammount of data (client -> server, server -> client)
// TODO: simultaneous send/receive for both client and server

// send data of size 0

// TODO: test with invalid ipv4 address and port

// investigate from libuv: test-tcp-write-to-half-open-connection.c
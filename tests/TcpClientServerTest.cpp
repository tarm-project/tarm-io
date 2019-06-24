#include "UTCommon.h"

#include "io/TcpClient.h"
#include "io/TcpServer.h"
#include "io/ScopeExitGuard.h"

#include <cstdint>
#include <thread>

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
    server.listen([&](io::TcpServer& server, io::TcpClient& client) -> bool {
        return true;
    },
    [&](io::TcpServer& server, io::TcpClient& client, const char* buf, size_t size) {
        data_received = true;
        std::string received_message(buf, size);

        EXPECT_EQ(size, message.length());
        EXPECT_EQ(received_message, message);
        server.shutdown();
    });

    bool data_sent = false;

    auto client = new io::TcpClient(loop);
    client->connect(m_default_addr,m_default_port, [&](io::TcpClient& client) {
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
    client_1->connect(m_default_addr, m_default_port, [&](io::TcpClient& client) {
        client.send_data(message + "1", [&](io::TcpClient& client) {
            data_sent_1 = true;
            client.schedule_removal();
        });
    },
    nullptr);

    bool data_sent_2 = false;

    auto client_2 = new io::TcpClient(loop);
    client_2->connect(m_default_addr, m_default_port, [&](io::TcpClient& client) {
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
        client->connect(m_default_addr, m_default_port, [&](io::TcpClient& client) {
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
    client->connect(m_default_addr, m_default_port, [&](io::TcpClient& client) {

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
        client->connect(m_default_addr, m_default_port, [&](io::TcpClient& client) {
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

// tripple send in a row (check that data will be sent sequential)

// TODO: client and server on separate threads

// TODO: double shutdown test
// TODO: shutdown not connected test
// TODO: send-receive large ammount of data (client -> server, server -> client)
// TODO: simultaneous send/receive for both client and server

// send data of size 0

// investigate from libuv: test-tcp-write-to-half-open-connection.c
// client connect to nonexisting server
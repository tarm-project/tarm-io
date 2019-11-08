
#include "UTCommon.h"

#include "io/UdpClient.h"
#include "io/UdpServer.h"
#include "io/ScopeExitGuard.h"
#include "io/Timer.h"

#include <cstdint>

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
        });

    ASSERT_EQ(0, loop.run());
}

// TODO: UDP client sending test with no destination set
// TODO: check address of UDP peer
// TODO: ensure that UDO client receives packets only from destination peer
// TODO: error on multiple start_receive on UDP server

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
    client->connect(m_default_addr, m_default_port, [&](io::TcpClient& client) {
        client.send_data(message, [&](io::TcpClient& client) {
            data_sent = true;
            client.schedule_removal();
        });
    });

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
    });

    bool data_sent_2 = false;

    auto client_2 = new io::TcpClient(loop);
    client_2->connect(m_default_addr, m_default_port, [&](io::TcpClient& client) {
        client.send_data(message + "2", [&](io::TcpClient& client) {
            data_sent_2 = true;
            client.schedule_removal();
        });
    });

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
        });

        EXPECT_EQ(0, loop.run());
        EXPECT_TRUE(send_called);
    });

    io::ScopeExitGuard guard([&server_thread, &client_thread](){
        client_thread.join();
        server_thread.join();
    });
}

// tripple send in a row (check that data will be sent sequential)

// TODO: client and server on separate threads
// TODO: server sends data to client frirs, right after connecting
// TODO: double shutdown test
// TODO: shutdown not connected test
// TODO: send-receive large ammount of data (client -> server, server -> client)
// TODO: simultaneous send/receive for both client and server


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

TEST_F(GenericMessageOrientedClientServerTest, default_state) {
    io::EventLoop loop;

    TcpClientPtr tcp_client(new io::net::TcpClient(loop), io::Removable::default_delete());
    TcpMessageOrientedClient message_client(std::move(tcp_client));

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

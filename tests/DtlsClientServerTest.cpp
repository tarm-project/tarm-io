#include "UTCommon.h"

#include "io/Path.h"
#include "io/DtlsClient.h"
#include "io/DtlsServer.h"

#include <boost/dll.hpp>

#include <thread>

struct DtlsClientServerTest : public testing::Test,
                              public LogRedirector {
protected:
    std::uint16_t m_default_port = 30541;
    std::string m_default_addr = "127.0.0.1";

    const io::Path m_test_path = boost::dll::program_location().parent_path().string();

    const io::Path m_cert_path = m_test_path / "certificate.pem";
    const io::Path m_key_path = m_test_path / "key.pem";
};

TEST_F(DtlsClientServerTest, client_without_actions) {
    io::EventLoop loop;

    auto client = new io::DtlsClient(loop);
    client->schedule_removal();

    ASSERT_EQ(0, loop.run());
}

TEST_F(DtlsClientServerTest, server_without_actions) {
    io::EventLoop loop;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    server->schedule_removal();

    ASSERT_EQ(0, loop.run());
}

TEST_F(DtlsClientServerTest, client_and_server_send_message_each_other) {
    io::EventLoop loop;

    const std::string client_message = "Hello from client!";
    const std::string server_message = "Hello from server!";

    std::size_t server_new_connection_counter = 0;
    std::size_t server_data_receive_counter = 0;
    std::size_t server_data_send_counter = 0;

    std::size_t client_new_connection_counter = 0;
    std::size_t client_data_receive_counter = 0;
    std::size_t client_data_send_counter = 0;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    server->listen(m_default_addr, m_default_port,
        [&](io::DtlsServer&, io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_new_connection_counter;
        },
        [&](io::DtlsServer&, io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_counter;

            std::string s(data.buf.get(), data.size);
            EXPECT_EQ(client_message, s);

            client.send_data(server_message,
                [&](io::DtlsConnectedClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string();
                    ++server_data_send_counter;
                }
            );
        }
    );

    auto client = new io::DtlsClient(loop);
    client->connect(m_default_addr, m_default_port,
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_new_connection_counter;

            client.send_data(client_message,
                [&](io::DtlsClient& client, const io::Error& error) {
                    EXPECT_FALSE(error);
                    ++client_data_send_counter;
                }
            );
        },
        [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            std::string s(data.buf.get(), data.size);
            EXPECT_EQ(server_message, s);
            ++client_data_receive_counter;

            // All interaction is done at this point
            server->schedule_removal();
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_new_connection_counter);
    EXPECT_EQ(0, server_data_receive_counter);
    EXPECT_EQ(0, server_data_send_counter);
    EXPECT_EQ(0, client_new_connection_counter);
    EXPECT_EQ(0, client_data_receive_counter);
    EXPECT_EQ(0, client_data_send_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, server_new_connection_counter);
    EXPECT_EQ(1, server_data_receive_counter);
    EXPECT_EQ(1, server_data_send_counter);
    EXPECT_EQ(1, client_new_connection_counter);
    EXPECT_EQ(1, client_data_receive_counter);
    EXPECT_EQ(1, client_data_send_counter);
}

// TODO: FIXME hang under valgrind sometimes
TEST_F(DtlsClientServerTest, client_and_server_in_threads_send_message_each_other) {
    // Note: pretty much the same test as 'client_and_server_send_message_each_other'
    // but in threads.

    const std::string server_message = "Hello from server!";
    const std::string client_message = "Hello from client!";

    std::size_t server_new_connection_counter = 0;
    std::size_t server_data_receive_counter = 0;
    std::size_t server_data_send_counter = 0;

    std::thread server_thread([&]() {
        io::EventLoop loop;

        auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
        server->listen(m_default_addr, m_default_port,
            [&](io::DtlsServer&, io::DtlsConnectedClient& client, const io::Error& error){
                EXPECT_FALSE(error);
                ++server_new_connection_counter;
            },
            [&](io::DtlsServer&, io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
                EXPECT_FALSE(error);
                ++server_data_receive_counter;

                std::string s(data.buf.get(), data.size);
                EXPECT_EQ(client_message, s);

                client.send_data(server_message,
                    [&](io::DtlsConnectedClient& client, const io::Error& error) {
                        EXPECT_FALSE(error) << error.string();
                        ++server_data_send_counter;
                        server->schedule_removal();
                    }
                );
            }
        );

        ASSERT_EQ(0, loop.run());
    });

    std::size_t client_new_connection_counter = 0;
    std::size_t client_data_receive_counter = 0;
    std::size_t client_data_send_counter = 0;

    std::thread client_thread([&]() {
        io::EventLoop loop;

        // Giving a time to start a server
        // TODO: de we need some callback to detect that server was started?
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto client = new io::DtlsClient(loop);
        client->connect(m_default_addr, m_default_port,
            [&](io::DtlsClient& client, const io::Error& error) {
                EXPECT_FALSE(error) << error.string();
                ++client_new_connection_counter;

                client.send_data(client_message,
                    [&](io::DtlsClient& client, const io::Error& error) {
                        EXPECT_FALSE(error);
                        ++client_data_send_counter;
                    }
                );
            },
            [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
                EXPECT_FALSE(error);

                std::string s(data.buf.get(), data.size);
                EXPECT_EQ(server_message, s);
                ++client_data_receive_counter;

                client.schedule_removal();
            }
        );

        ASSERT_EQ(0, loop.run());
    });

    server_thread.join();
    client_thread.join();

    EXPECT_EQ(1, server_new_connection_counter);
    EXPECT_EQ(1, server_data_receive_counter);
    EXPECT_EQ(1, server_data_send_counter);
    EXPECT_EQ(1, client_new_connection_counter);
    EXPECT_EQ(1, client_data_receive_counter);
    EXPECT_EQ(1, client_data_send_counter);
}

TEST_F(DtlsClientServerTest, client_send_1mb_chunk) {
    // Note: 1 mb cunks are larger than DTLS could handle
    io::EventLoop loop;

    const std::size_t LARGE_DATA_SIZE = 1024 * 1024;
    const std::size_t NORMAL_DATA_SIZE = 2 * 1024;

    std::shared_ptr<char> client_buf(new char[LARGE_DATA_SIZE], std::default_delete<char[]>());
    std::memset(client_buf.get(), 0, LARGE_DATA_SIZE);

    std::size_t client_data_send_counter = 0;
    std::size_t server_data_send_counter = 0;

    // A bit of callback hell here ]:-)
    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    server->listen(m_default_addr, m_default_port,
        nullptr,
        [&](io::DtlsServer&, io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            EXPECT_EQ(NORMAL_DATA_SIZE, data.size);

            client.send_data(client_buf, LARGE_DATA_SIZE,
                [&](io::DtlsConnectedClient& client, const io::Error& error) {
                    EXPECT_TRUE(error);
                    EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());

                    client.send_data(client_buf, LARGE_DATA_SIZE,
                        [&](io::DtlsConnectedClient& client, const io::Error& error) {
                            EXPECT_TRUE(error);
                            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());

                            client.send_data(client_buf, NORMAL_DATA_SIZE,
                                [&](io::DtlsConnectedClient& client, const io::Error& error) {
                                    EXPECT_FALSE(error);
                                    ++server_data_send_counter;
                                    server->schedule_removal();
                                }
                            );
                        }
                    );
                }
            );
        }
    );

    auto client = new io::DtlsClient(loop);
    client->connect(m_default_addr, m_default_port,
        [&](io::DtlsClient& client, const io::Error& error) {
            client.send_data(client_buf, LARGE_DATA_SIZE,
                [&](io::DtlsClient& client, const io::Error& error) {
                    EXPECT_TRUE(error);
                    EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());

                    client.send_data(client_buf, LARGE_DATA_SIZE,
                        [&](io::DtlsClient& client, const io::Error& error) {
                            EXPECT_TRUE(error);
                            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());

                            client.send_data(client_buf, NORMAL_DATA_SIZE,
                                [&](io::DtlsClient& client, const io::Error& error) {
                                    EXPECT_FALSE(error);
                                    ++client_data_send_counter;
                                }
                            );
                        }
                    );
                }
            );
        },
        [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            EXPECT_EQ(NORMAL_DATA_SIZE, data.size);
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_data_send_counter);
    EXPECT_EQ(0, server_data_send_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_data_send_counter);
    EXPECT_EQ(1, server_data_send_counter);
}

/*
TEST_F(DtlsClientServerTest, default_constructor) {
    io::EventLoop loop;
    this->log_to_stdout();

    auto client = new io::DtlsClient(loop);
    client->connect("51.15.68.114", 1234,
        [](io::DtlsClient& client, const io::Error&) {
            EXPECT_FALSE(error);
            std::cout << "Connected!!!" << std::endl;
            client.send_data("bla_bla_bla");
        },
        [](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            std::cout.write(buf, size);
        }
    );

    ASSERT_EQ(0, loop.run());
}

TEST_F(DtlsClientServerTest, default_constructor) {
    io::EventLoop loop;
    this->log_to_stdout();

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    server->listen("0.0.0.0", 1234,
        [&](io::DtlsServer&, io::DtlsConnectedClient& client){
            std::cout << "On new connection!!!" << std::endl;
            client.send_data("Hello world!\n");
        },
        [&](io::DtlsServer&, io::DtlsConnectedClient&, const io::DataChunk& data){
            std::cout.write(buf, size);
            server->schedule_removal();
        }
    );

    ASSERT_EQ(0, loop.run());
}
*/

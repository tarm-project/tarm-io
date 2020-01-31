#include "UTCommon.h"

#include "io/Path.h"
#include "io/DtlsClient.h"
#include "io/DtlsServer.h"
#include "io/global/Version.h"

#include <thread>

struct DtlsClientServerTest : public testing::Test,
                              public LogRedirector {
protected:
    std::uint16_t m_default_port = 30541;
    std::string m_default_addr = "127.0.0.1";

    const io::Path m_test_path = exe_path().string();

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
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(server, &client.server());
            ++server_new_connection_counter;
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
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

TEST_F(DtlsClientServerTest, connected_peer_timeout) {
    io::EventLoop loop;

    const std::size_t TIMEOUT_MS = 100;
    const std::string client_message = "Hello from client!";

    std::size_t server_new_connection_counter = 0;
    std::size_t server_data_receive_counter = 0;
    std::size_t server_peer_timeout_counter = 0;

    std::size_t client_new_connection_counter = 0;
    std::size_t client_data_receive_counter = 0;
    std::size_t client_data_send_counter = 0;

    std::chrono::high_resolution_clock::time_point t1;
    std::chrono::high_resolution_clock::time_point t2;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    server->listen(m_default_addr, m_default_port,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_new_connection_counter;
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_counter;
            t1 = std::chrono::high_resolution_clock::now();
        },
        TIMEOUT_MS,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_peer_timeout_counter;
            server->schedule_removal();

            t2 = std::chrono::high_resolution_clock::now();
            EXPECT_LE(TIMEOUT_MS - 5, std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
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
                    client.schedule_removal();
                }
            );
        },
        [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_data_receive_counter;
        }
    );

    EXPECT_EQ(0, server_new_connection_counter);
    EXPECT_EQ(0, server_data_receive_counter);
    EXPECT_EQ(0, server_peer_timeout_counter);
    EXPECT_EQ(0, client_new_connection_counter);
    EXPECT_EQ(0, client_data_receive_counter);
    EXPECT_EQ(0, client_data_send_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, server_new_connection_counter);
    EXPECT_EQ(1, server_data_receive_counter);
    EXPECT_EQ(1, server_peer_timeout_counter);
    EXPECT_EQ(1, client_new_connection_counter);
    EXPECT_EQ(0, client_data_receive_counter);
    EXPECT_EQ(1, client_data_send_counter);
}

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
            [&](io::DtlsConnectedClient& client, const io::Error& error){
                EXPECT_FALSE(error);
                ++server_new_connection_counter;
            },
            [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
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
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
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

TEST_F(DtlsClientServerTest, dtls_negotiated_version) {
    io::EventLoop loop;

    std::size_t client_new_connection_counter = 0;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    server->listen(m_default_addr, m_default_port,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(io::global::max_supported_dtls_version(), client.negotiated_dtls_version());
            server->schedule_removal();
        },
        nullptr
    );

    auto client = new io::DtlsClient(loop);
    EXPECT_EQ(io::DtlsVersion::UNKNOWN, client->negotiated_dtls_version());
    client->connect(m_default_addr, m_default_port,
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_new_connection_counter;
            EXPECT_EQ(io::global::max_supported_dtls_version(), client.negotiated_dtls_version());
            client.schedule_removal();
        },
        nullptr
    );

    EXPECT_EQ(0, client_new_connection_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_new_connection_counter);

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
        [&](io::DtlsConnectedClient& client){
            std::cout << "On new connection!!!" << std::endl;
            client.send_data("Hello world!\n");
        },
        [&](io::DtlsConnectedClient&, const io::DataChunk& data){
            std::cout.write(buf, size);
            server->schedule_removal();
        }
    );

    ASSERT_EQ(0, loop.run());
}
*/

TEST_F(DtlsClientServerTest, client_with_restricted_dtls_version) {
    const std::string message = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen(m_default_addr, m_default_port,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_connect_callback_count;
            EXPECT_EQ(io::global::min_supported_dtls_version(), client.negotiated_dtls_version());
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_callback_count;

            EXPECT_EQ(message.size(), data.size);
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ(message, received_message);

            server->schedule_removal();
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::DtlsClient(loop,
        io::DtlsVersionRange{io::global::min_supported_dtls_version(), io::global::min_supported_dtls_version()});

    client->connect(m_default_addr, m_default_port,
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(io::global::min_supported_dtls_version(), client.negotiated_dtls_version());
            ++client_on_connect_callback_count;
            client.send_data(message, [&](io::DtlsClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                ++client_on_send_callback_count;
                client.schedule_removal();
            });
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_send_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(1, client_on_send_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(1, server_on_receive_callback_count);
}

TEST_F(DtlsClientServerTest, server_with_restricted_dtls_version) {
    const std::string message = "Hello!";
    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_send_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path,
        io::DtlsVersionRange{io::global::min_supported_dtls_version(), io::global::min_supported_dtls_version()});
    auto listen_error = server->listen(m_default_addr, m_default_port,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_connect_callback_count;
            EXPECT_EQ(io::global::min_supported_dtls_version(), client.negotiated_dtls_version());
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_callback_count;

            EXPECT_EQ(message.size(), data.size);
            std::string received_message(data.buf.get(), data.size);
            EXPECT_EQ(message, received_message);

            server->schedule_removal();
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::DtlsClient(loop);

    client->connect(m_default_addr, m_default_port,
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(io::global::min_supported_dtls_version(), client.negotiated_dtls_version());
            ++client_on_connect_callback_count;
            client.send_data(message, [&](io::DtlsClient& client, const io::Error& error) {
                EXPECT_FALSE(error);
                ++client_on_send_callback_count;
                client.schedule_removal();
            });
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_send_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(1, client_on_send_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(1, server_on_receive_callback_count);
}

TEST_F(DtlsClientServerTest, client_and_server_dtls_version_mismatch) {
    if (io::global::min_supported_dtls_version() == io::global::max_supported_dtls_version()) {
        return;
    }

    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_receive_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path,
        io::DtlsVersionRange{io::global::max_supported_dtls_version(), io::global::max_supported_dtls_version()});
    auto listen_error = server->listen(m_default_addr, m_default_port,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            EXPECT_EQ(io::DtlsVersion::UNKNOWN, client.negotiated_dtls_version());
            ++server_on_connect_callback_count;
            server->schedule_removal();
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_callback_count;
        }
    );
    ASSERT_FALSE(listen_error);

    auto client = new io::DtlsClient(loop,
        io::DtlsVersionRange{io::global::min_supported_dtls_version(), io::global::min_supported_dtls_version()});
    client->connect(m_default_addr, m_default_port,
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            EXPECT_EQ(io::DtlsVersion::UNKNOWN, client.negotiated_dtls_version());
            ++client_on_connect_callback_count;
            client.schedule_removal();
        },
        [&](io::DtlsClient&, const io::DataChunk&, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_receive_callback_count;
        }
    );

    EXPECT_EQ(0, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_receive_callback_count);
    EXPECT_EQ(0, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_connect_callback_count);
    EXPECT_EQ(0, client_on_receive_callback_count);
    EXPECT_EQ(1, server_on_connect_callback_count);
    EXPECT_EQ(0, server_on_receive_callback_count);
}

TEST_F(DtlsClientServerTest, save_received_buffer) {
    io::EventLoop loop;

    const std::string client_message_1 = "1: Hello from client!";
    const std::string client_message_2 = "2: Hello from client!";
    const std::string server_message_1 = "1: Hello from server!";
    const std::string server_message_2 = "2: Hello from server!";

    //std::size_t server_new_connection_counter = 0;
    std::size_t server_data_receive_counter = 0;
    //std::size_t server_data_send_counter = 0;

    //std::size_t client_new_connection_counter = 0;
    std::size_t client_data_receive_counter = 0;
    //std::size_t client_data_send_counter = 0;

    std::shared_ptr<const char> client_saved_buf;
    std::shared_ptr<const char> server_saved_buf;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    server->listen(m_default_addr, m_default_port,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_counter;

            std::string s(data.buf.get(), data.size);
            if (server_data_receive_counter == 1) {
                EXPECT_EQ(client_message_1, s);
                server_saved_buf = data.buf;
            } else {
                EXPECT_EQ(client_message_2, s);
            }

            client.send_data((server_data_receive_counter == 1 ?
                              server_message_1 :
                              server_message_2));
        }
    );

    auto client = new io::DtlsClient(loop);
    client->connect(m_default_addr, m_default_port,
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();

            client.send_data(client_message_1);
        },
        [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            ++client_data_receive_counter;

            std::string s(data.buf.get(), data.size);
            if (server_data_receive_counter == 1) {
                EXPECT_EQ(server_message_1, s);
                client_saved_buf = data.buf;
                client.send_data(client_message_2);
            } else {
                EXPECT_EQ(server_message_2, s);

                // All interaction is done at this point
                server->schedule_removal();
                client.schedule_removal();
            }
        }
    );

    EXPECT_EQ(0, server_data_receive_counter);
    EXPECT_EQ(0, client_data_receive_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(2, server_data_receive_counter);
    EXPECT_EQ(2, client_data_receive_counter);

    // As we do not save size, reuse it from initial messages constants
    EXPECT_EQ(server_message_1, std::string(client_saved_buf.get(), server_message_1.size()));
    EXPECT_EQ(client_message_1, std::string(server_saved_buf.get(), client_message_1.size()));
}

// TODO: DTLS version lower is bigger than higher error

// TODO: unit test invalid address

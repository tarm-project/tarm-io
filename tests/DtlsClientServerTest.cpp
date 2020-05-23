/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "io/ByteSwap.h"
#include "io/DtlsClient.h"
#include "io/DtlsServer.h"
#include "io/Path.h"
#include "io/ScopeExitGuard.h"
#include "io/Timer.h"
#include "io/UdpClient.h"
#include "io/UdpServer.h"
#include "io/global/Version.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

// Just to include platform-specific networking headers
#include <uv.h>

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

TEST_F(DtlsClientServerTest, server_with_invalid_address) {
    io::EventLoop loop;

    std::size_t server_on_new_connection_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    auto error = server->listen({"", m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_new_connection_count;
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_count;
        },
        1000 * 100,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            ++server_on_close_count;
        }
    );

    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());

    server->schedule_removal();

    EXPECT_EQ(0, server_on_new_connection_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(0, server_on_new_connection_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);
}

#if defined(__APPLE__) || defined(__linux__)
// Windows does not have privileged ports
TEST_F(DtlsClientServerTest, bind_privileged) {
    io::EventLoop loop;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, 100},
        nullptr
    );
    EXPECT_TRUE(listen_error);
    EXPECT_EQ(io::StatusCode::PERMISSION_DENIED, listen_error.code());

    server->schedule_removal();

    ASSERT_EQ(0, loop.run());
}
#endif

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
    auto listen_error = server->listen({m_default_addr, m_default_port},
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
    ASSERT_FALSE(listen_error) << listen_error.string();

    auto client = new io::DtlsClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_new_connection_counter;

            client.send_data(client_message,
                [&](io::DtlsClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string();
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
    server->listen({m_default_addr, m_default_port},
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
            EXPECT_TIMEOUT_MS(TIMEOUT_MS, std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1));
        }
    );

    auto client = new io::DtlsClient(loop);
    client->connect({m_default_addr, m_default_port},
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
        server->listen({m_default_addr, m_default_port},
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
        client->connect({m_default_addr, m_default_port},
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
    server->listen({m_default_addr, m_default_port},
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
    client->connect({m_default_addr, m_default_port},
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
    server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            EXPECT_EQ(io::global::max_supported_dtls_version(), client.negotiated_dtls_version());
            server->schedule_removal();
        },
        nullptr
    );

    auto client = new io::DtlsClient(loop);
    EXPECT_EQ(io::DtlsVersion::UNKNOWN, client->negotiated_dtls_version());
    client->connect({m_default_addr, m_default_port},
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
    auto listen_error = server->listen({m_default_addr, m_default_port},
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

    client->connect({m_default_addr, m_default_port},
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
    auto listen_error = server->listen({m_default_addr, m_default_port},
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

    client->connect({m_default_addr, m_default_port},
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
        IO_TEST_SKIP();
    }

    std::size_t client_on_connect_callback_count = 0;
    std::size_t client_on_receive_callback_count = 0;
    std::size_t server_on_connect_callback_count = 0;
    std::size_t server_on_receive_callback_count = 0;

    io::EventLoop loop;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path,
        io::DtlsVersionRange{io::global::max_supported_dtls_version(), io::global::max_supported_dtls_version()});
    auto listen_error = server->listen({m_default_addr, m_default_port},
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
    client->connect({m_default_addr, m_default_port},
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

    std::size_t server_data_receive_counter = 0;
    std::size_t client_data_receive_counter = 0;

    std::shared_ptr<const char> client_saved_buf;
    std::shared_ptr<const char> server_saved_buf;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    server->listen({m_default_addr, m_default_port},
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
    client->connect({m_default_addr, m_default_port},
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

TEST_F(DtlsClientServerTest, fail_to_init_ssl_on_client) {
    // Note: in this test we set invalid ciphers list -> SSL is not able to init

    io::EventLoop loop;

    std::size_t server_connect_counter = 0;
    std::size_t server_data_receive_counter = 0;

    std::size_t client_data_receive_counter = 0;
    std::size_t client_connect_counter = 0;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_connect_counter;
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_data_receive_counter;
        }
    );

    const auto previous_ciphers = io::global::ciphers_list();
    io::global::set_ciphers_list("!@#$%^&*()");
    io::ScopeExitGuard scope_guard([=]() {
        io::global::set_ciphers_list(previous_ciphers);
    });

    auto client = new io::DtlsClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            ++client_connect_counter;
            client.schedule_removal();
            server->schedule_removal();
        },
        [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_data_receive_counter;
        }
    );

    EXPECT_EQ(0, server_connect_counter);
    EXPECT_EQ(0, server_data_receive_counter);
    EXPECT_EQ(0, client_data_receive_counter);
    EXPECT_EQ(0, client_connect_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(0, server_connect_counter);
    EXPECT_EQ(0, server_data_receive_counter);
    EXPECT_EQ(0, client_data_receive_counter);
    EXPECT_EQ(1, client_connect_counter);
}

TEST_F(DtlsClientServerTest, close_connection_from_server_side) {
    io::EventLoop loop;

    std::size_t client_on_close_count = 0;
    std::size_t client_on_receive_count = 0;

    const std::string client_message = "Hello from client!";
    const std::string server_message = "Hello from server!";

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);

            client.send_data(server_message,
                [&](io::DtlsConnectedClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string();
                    client.close();
                }
            );
        }
    );

    auto client = new io::DtlsClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            client.send_data(client_message,
                [&](io::DtlsClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string();
                }
            );
        },
        [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            ++client_on_receive_count;
        },
        [&](io::DtlsClient& client, const io::Error& error) {
            server->schedule_removal();
            client.schedule_removal();

            ++client_on_close_count;
        }
    );

    EXPECT_EQ(0, client_on_close_count);
    EXPECT_EQ(0, client_on_receive_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_close_count);
    EXPECT_EQ(1, client_on_receive_count);
}

TEST_F(DtlsClientServerTest, close_connection_from_client_side_with_no_data_sent) {
    io::EventLoop loop;

    std::size_t client_on_new_connection_count = 0;
    std::size_t client_on_receive_count = 0;
    std::size_t client_on_close_count = 0;

    std::size_t server_on_new_connection_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_new_connection_count;
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            // Not expecting this call
            ++server_on_receive_count;
        },
        1000 * 100,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            ++server_on_close_count;
            server->schedule_removal();
        }
    );

    auto client = new io::DtlsClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_on_new_connection_count;
            client.close();
        },
        [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            // Not expecting this call
            ++client_on_receive_count;
        },
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_close_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_on_new_connection_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(0, client_on_close_count);
    EXPECT_EQ(0, server_on_new_connection_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_new_connection_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(1, client_on_close_count);
    EXPECT_EQ(1, server_on_new_connection_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(1, server_on_close_count);
}

TEST_F(DtlsClientServerTest, close_connection_from_client_side_with_with_data_sent) {
    io::EventLoop loop;

    const std::vector<std::string> client_data_to_send = {
        "a", "b", "c", "d", "e", "f", "g", "h", "i"
    };

    std::size_t client_on_new_connection_count = 0;
    std::size_t client_on_receive_count = 0;
    std::size_t client_on_close_count = 0;

    std::size_t server_on_new_connection_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_new_connection_count;
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_count;
        },
        1000 * 100,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            ++server_on_close_count;
            server->schedule_removal();
        }
    );

    auto client = new io::DtlsClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_on_new_connection_count;
            for (auto v : client_data_to_send) {
                client.send_data(v, [=, &client_data_to_send](io::DtlsClient& client, const io::Error& error) {
                    EXPECT_FALSE(error) << error.string();
                    if (v == client_data_to_send.back()) {
                        client.close();
                    }
                });
            }
        },
        [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            // Not expecting this call
            ++client_on_receive_count;
        },
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++client_on_close_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_on_new_connection_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(0, client_on_close_count);
    EXPECT_EQ(0, server_on_new_connection_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_new_connection_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(1, client_on_close_count);
    EXPECT_EQ(1, server_on_new_connection_count);
    EXPECT_EQ(client_data_to_send.size(), server_on_receive_count);
    EXPECT_EQ(1, server_on_close_count);
}

TEST_F(DtlsClientServerTest, client_with_invalid_dtls_version) {
    if (io::global::min_supported_dtls_version() == io::global::max_supported_dtls_version()) {
        IO_TEST_SKIP();
    }

    std::size_t client_on_connect_count = 0;

    // Note: Min > Max in this test
    io::EventLoop loop;
    auto client = new io::DtlsClient(loop,
        io::DtlsVersionRange{io::global::max_supported_dtls_version(),
                             io::global::min_supported_dtls_version()});

    client->connect({m_default_addr, m_default_port},
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            ++client_on_connect_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_on_connect_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_connect_count);
}

TEST_F(DtlsClientServerTest, server_with_invalid_dtls_version) {
    if (io::global::min_supported_dtls_version() == io::global::max_supported_dtls_version()) {
        IO_TEST_SKIP();
    }

    std::size_t server_on_new_connection_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;

    // Note: Min > Max in this test
    io::EventLoop loop;
    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path,
        io::DtlsVersionRange{io::global::max_supported_dtls_version(),
                             io::global::min_supported_dtls_version()});
    auto error = server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_new_connection_count;
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error);
            ++server_on_receive_count;
        },
        1000 * 100,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            ++server_on_close_count;
        }
    );

    EXPECT_TRUE(error);
    EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());

    server->schedule_removal();

    EXPECT_EQ(0, server_on_new_connection_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(0, server_on_new_connection_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);
}

TEST_F(DtlsClientServerTest, client_with_invalid_address) {
    std::size_t client_on_connect_count = 0;

    io::EventLoop loop;
    auto client = new io::DtlsClient(loop);

    client->connect({"0.0", m_default_port},
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
            ++client_on_connect_count;
            client.schedule_removal();
        }
    );

    EXPECT_EQ(0, client_on_connect_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_connect_count);
}

TEST_F(DtlsClientServerTest, server_address_in_use) {
    io::EventLoop loop;

    auto server_1 = new io::DtlsServer(loop, m_cert_path, m_key_path);
    auto listen_error_1 = server_1->listen({m_default_addr, m_default_port}, nullptr);
    EXPECT_FALSE(listen_error_1);

    auto server_2 = new io::DtlsServer(loop, m_cert_path, m_key_path);
    auto listen_error_2 = server_2->listen({m_default_addr, m_default_port}, nullptr);
    EXPECT_TRUE(listen_error_2);
    EXPECT_EQ(io::StatusCode::ADDRESS_ALREADY_IN_USE, listen_error_2.code());

    server_1->schedule_removal();
    server_2->schedule_removal();

    ASSERT_EQ(0, loop.run());
}

TEST_F(DtlsClientServerTest, client_with_invalid_address_and_no_connect_callback) {
    // Note: crash test
    io::EventLoop loop;
    auto client = new io::DtlsClient(loop);

    client->connect({"0.0", m_default_port},
        nullptr
    );
    client->schedule_removal();

    ASSERT_EQ(0, loop.run());
}

TEST_F(DtlsClientServerTest, client_no_timeout_on_data_send) {
    io::EventLoop loop;

    const std::size_t COMMON_TIMEOUT_MS = 200;
    const std::size_t SEND_TIMEOUT_MS = 150;

    const auto start_time = std::chrono::system_clock::now();

    std::size_t client_on_close_callback_count = 0;
    std::size_t server_on_close_callback_count = 0;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            // Note: here we capture client by reference in Timer's callback. This should not
            //       be done in production code because it is impossible to track lifetime of
            //       server-size objects outside of the server callbacks.
            (new io::Timer(loop))->start(SEND_TIMEOUT_MS,
                [&](io::Timer& timer) {
                    client.send_data("!");
                    timer.schedule_removal();
                }
            );
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();

            client.close();
        },
        COMMON_TIMEOUT_MS,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_close_callback_count;

            const auto end_time = std::chrono::system_clock::now();
            EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count(),
                      2 * SEND_TIMEOUT_MS);

            server->schedule_removal();
        }
    );
    ASSERT_FALSE(listen_error) << listen_error.string();

    auto client = new io::DtlsClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();

            (new io::Timer(loop))->start(SEND_TIMEOUT_MS,
                [&](io::Timer& timer) {
                    client.send_data("!");
                    timer.schedule_removal();
                }
            );
        },
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_on_close_callback_count;

            const auto end_time = std::chrono::system_clock::now();
            EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count(),
                      2 * SEND_TIMEOUT_MS);

            client.schedule_removal();
        },
        COMMON_TIMEOUT_MS
    );

    EXPECT_EQ(0, client_on_close_callback_count);
    EXPECT_EQ(0, server_on_close_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_close_callback_count);
    EXPECT_EQ(1, server_on_close_callback_count);
}

TEST_F(DtlsClientServerTest, client_timeout_cause_server_peer_close) {
    io::EventLoop loop;

    const std::size_t SERVER_TIMEOUT_MS = 300;
    const std::size_t CLIENT_TIMEOUT_MS = 200;

    std::size_t client_on_close_callback_count = 0;
    std::size_t server_on_close_callback_count = 0;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        SERVER_TIMEOUT_MS,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();

            EXPECT_EQ(1, client_on_close_callback_count);
            EXPECT_EQ(0, server_on_close_callback_count);
            ++server_on_close_callback_count;

            server->schedule_removal();
        }
    );
    ASSERT_FALSE(listen_error) << listen_error.string();

    auto client = new io::DtlsClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();

            EXPECT_EQ(0, client_on_close_callback_count);
            EXPECT_EQ(0, server_on_close_callback_count);
            ++client_on_close_callback_count;

            client.schedule_removal();
        },
        CLIENT_TIMEOUT_MS
    );

    EXPECT_EQ(0, client_on_close_callback_count);
    EXPECT_EQ(0, server_on_close_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_close_callback_count);
    EXPECT_EQ(1, server_on_close_callback_count);
}

TEST_F(DtlsClientServerTest, server_peer_timeout_cause_client_close) {
    io::EventLoop loop;

    const std::size_t SERVER_TIMEOUT_MS = 200;
    const std::size_t CLIENT_TIMEOUT_MS = 60000;

    std::size_t client_on_close_callback_count = 0;
    std::size_t server_on_close_callback_count = 0;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        SERVER_TIMEOUT_MS,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();

            EXPECT_EQ(0, client_on_close_callback_count);
            EXPECT_EQ(0, server_on_close_callback_count);
            ++server_on_close_callback_count;

            server->schedule_removal();
        }
    );
    ASSERT_FALSE(listen_error) << listen_error.string();

    auto client = new io::DtlsClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
        },
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();

            EXPECT_EQ(0, client_on_close_callback_count);
            EXPECT_EQ(1, server_on_close_callback_count);
            ++client_on_close_callback_count;

            client.schedule_removal();
        },
        CLIENT_TIMEOUT_MS
    );

    EXPECT_EQ(0, client_on_close_callback_count);
    EXPECT_EQ(0, server_on_close_callback_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, client_on_close_callback_count);
    EXPECT_EQ(1, server_on_close_callback_count);
}

TEST_F(DtlsClientServerTest, server_schedule_removal_cause_client_close) {
    std::size_t server_on_connect_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;

    std::size_t client_on_connect_count = 0;
    std::size_t client_on_receive_count = 0;
    std::size_t client_on_close_count = 0;

    io::EventLoop loop;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_connect_count;
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_receive_count;
            server->schedule_removal();
        },
        100000,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_close_count;
        }
    );
    ASSERT_FALSE(listen_error) << listen_error.string();

    auto client = new io::DtlsClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            client.send_data("Hello!");
            ++client_on_connect_count;
        },
        [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_on_receive_count;
        },
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_on_close_count;
            client.schedule_removal();
        },
        100000
    );

    EXPECT_EQ(0, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);

    EXPECT_EQ(0, client_on_connect_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(0, client_on_close_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, server_on_connect_count);
    EXPECT_EQ(1, server_on_receive_count);
    EXPECT_EQ(1, server_on_close_count);

    EXPECT_EQ(1, client_on_connect_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(1, client_on_close_count);
}

TEST_F(DtlsClientServerTest, client_send_invalid_data_before_handshake) {
    std::size_t server_on_connect_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;

    std::size_t udp_on_receive_count = 0;

    io::EventLoop loop;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_TRUE(error);

            ++server_on_connect_count;

            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            server->schedule_removal();
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_receive_count;
        },
        100000,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_close_count;
        }
    );
    ASSERT_FALSE(listen_error) << listen_error.string();

    auto client = new io::UdpClient(loop);
    ASSERT_FALSE(client->set_destination({m_default_addr, m_default_port}));
    auto client_receive_start_error = client->start_receive(
        [&](io::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++udp_on_receive_count;
        }
    );
    ASSERT_FALSE(client_receive_start_error);

    client->send_data("!!!");

    (new io::Timer(loop))->start(100,
        [&](io::Timer& timer){
            client->schedule_removal();
            timer.schedule_removal();
        }
    );

    EXPECT_EQ(0, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);
    EXPECT_EQ(0, udp_on_receive_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);
    EXPECT_EQ(0, udp_on_receive_count);
}

const unsigned char DTLS_1_2_CLIENT_HELLO[] = {
    0x16, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf3, 0x01, 0x00, 0x00,
    0xfb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe7, 0xfe, 0xfd, 0xfc, 0xf0, 0x71, 0xa9, 0x8f,
    0x57, 0xd1, 0x32, 0x7a, 0xe1, 0x15, 0x2e, 0x3f, 0xa8, 0xe0, 0x73, 0xab, 0xa0, 0xf3, 0xd7, 0x02,
    0xd2, 0x84, 0x36, 0x61, 0xbe, 0x78, 0xef, 0xe2, 0x03, 0x94, 0x7c, 0x00, 0x00, 0x00, 0x7c, 0xc0,
    0x30, 0xc0, 0x2c, 0xc0, 0x14, 0x00, 0xa5, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x9f, 0x00, 0x39, 0x00,
    0x38, 0x00, 0x37, 0x00, 0x36, 0x00, 0x88, 0x00, 0x87, 0x00, 0x86, 0x00, 0x85, 0xc0, 0x19, 0xc0,
    0x32, 0xc0, 0x2e, 0xc0, 0x0f, 0xc0, 0x05, 0x00, 0x9d, 0x00, 0x35, 0x00, 0x84, 0xc0, 0x2f, 0xc0,
    0x2b, 0xc0, 0x13, 0x00, 0xa4, 0x00, 0xa2, 0x00, 0xa0, 0x00, 0x9e, 0x00, 0x33, 0x00, 0x32, 0x00,
    0x31, 0x00, 0x30, 0x00, 0x9a, 0x00, 0x99, 0x00, 0x98, 0x00, 0x97, 0x00, 0x45, 0x00, 0x44, 0x00,
    0x43, 0x00, 0x42, 0xc0, 0x18, 0xc0, 0x31, 0xc0, 0x2d, 0xc0, 0x0e, 0xc0, 0x04, 0x00, 0x9c, 0x00,
    0x2f, 0x00, 0x96, 0x00, 0x41, 0x00, 0x07, 0xc0, 0x12, 0x00, 0x16, 0x00, 0x13, 0x00, 0x10, 0x00,
    0x0d, 0xc0, 0x17, 0xc0, 0x0d, 0xc0, 0x03, 0x00, 0x0a, 0x00, 0xff, 0x01, 0x00, 0x00, 0x55, 0x00,
    0x0b, 0x00, 0x04, 0x03, 0x00, 0x01, 0x02, 0x00, 0x0a, 0x00, 0x1c, 0x00, 0x1a, 0x00, 0x17, 0x00,
    0x19, 0x00, 0x1c, 0x00, 0x1b, 0x00, 0x18, 0x00, 0x1a, 0x00, 0x16, 0x00, 0x0e, 0x00, 0x0d, 0x00,
    0x0b, 0x00, 0x0c, 0x00, 0x09, 0x00, 0x0a, 0x00, 0x23, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x20, 0x00,
    0x1e, 0x06, 0x01, 0x06, 0x02, 0x06, 0x03, 0x05, 0x01, 0x05, 0x02, 0x05, 0x03, 0x04, 0x01, 0x04,
    0x16, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x20, 0x01, 0x00, 0x00,
    0xfb, 0x00, 0x00, 0x00, 0x00, 0xe7, 0x00, 0x00, 0x14, 0x02, 0x04, 0x03, 0x03, 0x01, 0x03, 0x02,
    0x03, 0x03, 0x02, 0x01, 0x02, 0x02, 0x02, 0x03, 0x00, 0x0f, 0x00, 0x01, 0x01
};

TEST_F(DtlsClientServerTest, client_send_invalid_data_during_handshake) {
    std::size_t server_on_connect_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;

    std::size_t udp_on_receive_count = 0;

    io::EventLoop loop;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_TRUE(error);

            ++server_on_connect_count;

            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            server->schedule_removal();
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_receive_count;
        },
        100000,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_close_count;
        }
    );
    ASSERT_FALSE(listen_error) << listen_error.string();


    auto client = new io::UdpClient(loop);
    ASSERT_FALSE(client->set_destination({m_default_addr, m_default_port}));
    auto client_receive_start_error = client->start_receive(
        [&](io::UdpClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++udp_on_receive_count;
            EXPECT_GE(data.size, 50);
            EXPECT_EQ(0x16, data.buf.get()[0]); // Type = handshake
            EXPECT_EQ(char(0xFE), data.buf.get()[1]); // Common prefix of DTLS version

            client.send_data("!!!");

            (new io::Timer(loop))->start(100,
                [&](io::Timer& timer){
                    client.schedule_removal();
                    timer.schedule_removal();
                }
            );
        }
    );
    ASSERT_FALSE(client_receive_start_error);

    std::shared_ptr<const char> buf_ptr(reinterpret_cast<const char*>(DTLS_1_2_CLIENT_HELLO), [](const char*){});
    client->send_data(buf_ptr, sizeof(DTLS_1_2_CLIENT_HELLO));

    EXPECT_EQ(0, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);
    EXPECT_EQ(0, udp_on_receive_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);
    EXPECT_EQ(1, udp_on_receive_count);
}

// TODO: currently incompatible with TLS 1.3 implementation
TEST_F(DtlsClientServerTest, DISABLED_client_send_invalid_data_after_handshake) {
    // Note:  in this test we make regular handshake and then bind UDP socket with SO_REUSEADDR
    //        and send trash data.
    std::size_t server_on_connect_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;

    std::size_t client_on_connect_count = 0;
    std::size_t client_on_receive_count = 0;
    std::size_t client_on_close_count = 0;

    io::EventLoop loop;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_connect_count;
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            ++server_on_receive_count;
            server->schedule_removal();
        },
        100000,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_close_count;
        }
    );
    ASSERT_FALSE(listen_error) << listen_error.string();

    auto client = new io::DtlsClient(loop);
    client->connect({m_default_addr, m_default_port},
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_on_connect_count;

            /*
            // Just raw libuv code to send UDP data
            // TODO: looks like it could be rewritten with raw sockets
            ::sockaddr_in sock_addr{0};
            sock_addr.sin_family = AF_INET;
            sock_addr.sin_port = io::host_to_network(client.bound_port());
            uv_udp_t udp_handle;
            const io::Error init_error = uv_udp_init(reinterpret_cast<uv_loop_t*>(loop.raw_loop()), &udp_handle);

            const io::Error bind_error = uv_udp_bind(&udp_handle, reinterpret_cast<const ::sockaddr*>(&sock_addr), UV_UDP_REUSEADDR);
            if (bind_error) {
                FAIL() << "bind error: " << bind_error.string();
                return;
            }

            uv_udp_send_t send_request;
            uv_buf_t uv_buf = uv_buf_init(&invalid_message, 1);

            ::sockaddr_in destination{0};
            destination.sin_family = AF_INET;
            destination.sin_addr.s_addr = std::uint32_t(1) << 24 |
                                          std::uint32_t(0) << 16 |
                                          std::uint32_t(0) << 8  |
                                          std::uint32_t(127);
            destination.sin_port = io::host_to_network(m_default_port);

            int uv_status = uv_udp_send(&send_request,
                                        &udp_handle,
                                        &uv_buf,
                                        1,
                                        reinterpret_cast<const struct sockaddr*>(&destination),
                                        nullptr); // add on_send, and close UDP handle there
            if (uv_status < 0) {
                FAIL() << "send error: " << io::Error(uv_status).string();
                return;
            }
            */

            auto socket_handle = ::socket(AF_INET, SOCK_DGRAM, 0);
            ASSERT_GT(socket_handle, 0);

            int result = 0;
            int yes = 1;
#ifdef __APPLE__
            result = ::setsockopt(socket_handle, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(int));
#elif defined(_WIN32)
			result = ::setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(int));
#else
            result = ::setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
#endif
            ASSERT_NE(-1, result);

            ::sockaddr_in source_addr{0};
            source_addr.sin_family = AF_INET;
            source_addr.sin_port = io::host_to_network(client.bound_port());
            result = ::bind(socket_handle, reinterpret_cast<sockaddr*>(&source_addr), sizeof(source_addr));
            ASSERT_NE(-1, result);

            ::sockaddr_in dest_addr{0};
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_addr.s_addr = std::uint32_t(1) << 24 | // 127.0.0.1
                                        std::uint32_t(0) << 16 |
                                        std::uint32_t(0) << 8  |
                                        std::uint32_t(127);
            dest_addr.sin_port = io::host_to_network(m_default_port);

            result = ::sendto(socket_handle, "!!!", 3, 0, reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr));
            ASSERT_NE(-1, result);

            ::close(socket_handle);
        },
        [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_on_receive_count;
        },
        [&](io::DtlsClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++client_on_close_count;
            client.schedule_removal();
        },
        100000
    );

    EXPECT_EQ(0, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);

    EXPECT_EQ(0, client_on_connect_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(0, client_on_close_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, server_on_connect_count);
    EXPECT_EQ(1, server_on_receive_count);
    EXPECT_EQ(1, server_on_close_count);

    EXPECT_EQ(1, client_on_connect_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(1, client_on_close_count);
}

TEST_F(DtlsClientServerTest, client_send_data_to_server_after_connection_timeout_at_server) {
    std::size_t server_on_connect_count = 0;
    std::size_t server_on_receive_count = 0;
    std::size_t server_on_close_count = 0;

    std::size_t client_on_connect_count = 0;
    std::size_t client_on_receive_count = 0;
    std::size_t client_on_close_count = 0;

    io::EventLoop loop;

    auto server = new io::DtlsServer(loop, m_cert_path, m_key_path);
    auto listen_error = server->listen({m_default_addr, m_default_port},
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_connect_count;
        },
        [&](io::DtlsConnectedClient& client, const io::DataChunk& data, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::OPENSSL_ERROR, error.code());
            ++server_on_receive_count;
        },
        200,
        [&](io::DtlsConnectedClient& client, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            ++server_on_close_count;
        }
    );
    ASSERT_FALSE(listen_error) << listen_error.string();

    auto client = new io::DtlsClient(loop);
        client->connect({m_default_addr, m_default_port},
            [&](io::DtlsClient& client, const io::Error& error) {
                EXPECT_FALSE(error) << error.string();
                ++client_on_connect_count;
                (new io::Timer(loop))->start(400,
                    [&](io::Timer& timer) {
                        // TODO: currently this data will not be received because UDP peers at UDP server
                        //       have inactivity timeout which is hardcoded in DTLS server and filters incoming data.
                        //       Need to revise this/
                        client.send_data("!!!");
                        timer.schedule_removal();
                    }
                );

                (new io::Timer(loop))->start(600,
                    [&](io::Timer& timer) {
                        client.schedule_removal();
                        server->schedule_removal();
                        timer.schedule_removal();
                    }
                );
            },
            [&](io::DtlsClient& client, const io::DataChunk& data, const io::Error& error) {
                EXPECT_FALSE(error) << error.string();
                ++client_on_receive_count;
            },
            [&](io::DtlsClient& client, const io::Error& error) {
                EXPECT_FALSE(error) << error.string();
                EXPECT_EQ(0, client_on_close_count);
                EXPECT_EQ(1, server_on_close_count);
                ++client_on_close_count;
            },
            100000
        );

    EXPECT_EQ(0, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(0, server_on_close_count);

    EXPECT_EQ(0, client_on_connect_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(0, client_on_close_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, server_on_connect_count);
    EXPECT_EQ(0, server_on_receive_count);
    EXPECT_EQ(1, server_on_close_count);

    EXPECT_EQ(1, client_on_connect_count);
    EXPECT_EQ(0, client_on_receive_count);
    EXPECT_EQ(1, client_on_close_count);
}

TEST_F(DtlsClientServerTest, client_send_data_to_server_after_connection_timeout_at_client) {
    // TODO:
}

// TODO: key and certificate mismatch
// TODO: send data to server after connection timeout
// TODO: send data to client after connection timeout
// TODO: schedulre client for removal and at the same time send data from server (client should not call receive callback)

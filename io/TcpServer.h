#pragma once

#include "Common.h"
#include "TcpClient.h"

// TODO: forward declare this???
#include "EventLoop.h"

#include <boost/pool/pool.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>

namespace io {

class TcpServer {
public:
    friend class TcpClient;

    static const size_t READ_BUFFER_SIZE = 65536;

    // TODO: replace 'int status' with some error
    using NewConnectionCallback = std::function<bool(TcpServer&, TcpClient&)>;
    using DataReceivedCallback = std::function<void(TcpServer&, TcpClient&, const char*, std::size_t)>;

    TcpServer(EventLoop& loop);
    ~TcpServer(); // TODO: need to test if correct shutdown works in case of destruction of the server

    // TODO: some sort of macro here???
    TcpServer(const TcpServer& other) = delete;
    TcpServer& operator=(const TcpServer& other) = delete;

    TcpServer(TcpServer&& other) = default;
    TcpServer& operator=(TcpServer&& other) = delete; // default

    // TODO: need to return some enum (or StatusCode) instead of int
    int bind(const std::string& ip_addr_str, std::uint16_t port);

    // On success, zero is returned
    int listen(NewConnectionCallback new_connection_callback,
               DataReceivedCallback data_receive_callback,
               int backlog_size = 128);

    void shutdown();
    void close();

    std::size_t connected_clients_count() const;

private:
    void remove_client_connection(TcpClient* client);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

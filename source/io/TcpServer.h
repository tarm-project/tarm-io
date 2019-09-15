#pragma once

#include "EventLoop.h"
#include "Export.h"
#include "TcpConnectedClient.h"
#include "UserDataHolder.h"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>

namespace io {

class TcpConnectedClient;

class TcpServer : public UserDataHolder {
public:
    friend class TcpConnectedClient;

    static const size_t READ_BUFFER_SIZE = 65536;

    // TODO: replace 'int status' with some error
    using NewConnectionCallback = std::function<bool(TcpServer&, TcpConnectedClient&)>;
    using DataReceivedCallback = std::function<void(TcpServer&, TcpConnectedClient&, const char*, std::size_t)>;

    IO_DLL_PUBLIC TcpServer(EventLoop& loop);
    IO_DLL_PUBLIC ~TcpServer(); // TODO: need to test if correct shutdown works in case of destruction of the server

    // TODO: some sort of macro here???
    TcpServer(const TcpServer& other) = delete;
    TcpServer& operator=(const TcpServer& other) = delete;

    TcpServer(TcpServer&& other) = default;
    TcpServer& operator=(TcpServer&& other) = delete; // default

    IO_DLL_PUBLIC Error bind(const std::string& ip_addr_str, std::uint16_t port);

    // On success, zero is returned
    IO_DLL_PUBLIC
    int listen(NewConnectionCallback new_connection_callback,
               DataReceivedCallback data_receive_callback,
               int backlog_size = 128);

    IO_DLL_PUBLIC void shutdown();
    IO_DLL_PUBLIC void close();

    IO_DLL_PUBLIC std::size_t connected_clients_count() const;

private:
    void remove_client_connection(TcpConnectedClient* client);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

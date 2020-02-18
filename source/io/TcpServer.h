#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "EventLoop.h"
#include "Export.h"
#include "Removable.h"
#include "TcpConnectedClient.h"
#include "UserDataHolder.h"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>

namespace io {

class TcpConnectedClient;

class TcpServer : public Removable,
                  public UserDataHolder {
public:
    friend class TcpConnectedClient;

    static const size_t READ_BUFFER_SIZE = 65536;

    using NewConnectionCallback = std::function<void(TcpConnectedClient&, const Error&)>;
    using DataReceivedCallback = std::function<void(TcpConnectedClient&, const DataChunk&, const Error&)>;
    using CloseConnectionCallback = std::function<void(TcpConnectedClient&, const Error&)>;

    // TODO: test all move constructors
    IO_FORBID_COPY(TcpServer);
    IO_FORBID_MOVE(TcpServer);

    IO_DLL_PUBLIC TcpServer(EventLoop& loop);
    IO_DLL_PUBLIC ~TcpServer(); // TODO: need to test if correct shutdown works in case of destruction of the server

    IO_DLL_PUBLIC
    Error listen(const std::string& ip_addr_str,
                 std::uint16_t port,
                 NewConnectionCallback new_connection_callback,
                 DataReceivedCallback data_receive_callback,
                 CloseConnectionCallback close_connection_callback,
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

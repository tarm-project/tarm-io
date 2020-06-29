/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "net/Endpoint.h"
#include "EventLoop.h"
#include "Export.h"
#include "Removable.h"
#include "net/TcpConnectedClient.h"
#include "UserDataHolder.h"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>

namespace tarm {
namespace io {
namespace net {

class TcpConnectedClient;

class TcpServer : public Removable,
                  public UserDataHolder {
public:
    friend class TcpConnectedClient;

    static const size_t READ_BUFFER_SIZE = 65536;

    using NewConnectionCallback = std::function<void(TcpConnectedClient&, const Error&)>;
    using DataReceivedCallback = std::function<void(TcpConnectedClient&, const DataChunk&, const Error&)>;
    using CloseConnectionCallback = std::function<void(TcpConnectedClient&, const Error&)>;

    using CloseServerCallback = std::function<void(TcpServer&, const Error&)>;
    using ShutdownServerCallback = std::function<void(TcpServer&, const Error&)>;

    IO_FORBID_COPY(TcpServer);
    IO_FORBID_MOVE(TcpServer);

    IO_DLL_PUBLIC TcpServer(EventLoop& loop);

    IO_DLL_PUBLIC
    Error listen(const Endpoint& endpoint,
                 const NewConnectionCallback& new_connection_callback,
                 const DataReceivedCallback& data_receive_callback,
                 const CloseConnectionCallback& close_connection_callback,
                 int backlog_size = 128);

    IO_DLL_PUBLIC void close(const CloseServerCallback& close_callback = nullptr);
    IO_DLL_PUBLIC void shutdown(const ShutdownServerCallback& shutdown_callback = nullptr);

    IO_DLL_PUBLIC std::size_t connected_clients_count() const;

    IO_DLL_PUBLIC void schedule_removal() override;

    IO_DLL_PUBLIC const Endpoint& endpoint() const;

protected:
    IO_DLL_PUBLIC ~TcpServer();

private:
    void remove_client_connection(TcpConnectedClient* client);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace net
} // namespace io
} // namespace tarm

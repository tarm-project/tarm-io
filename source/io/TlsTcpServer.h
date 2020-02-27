#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "Endpoint.h"
#include "Error.h"
#include "EventLoop.h"
#include "Export.h"
#include "Path.h"
#include "Removable.h"
#include "TlsTcpConnectedClient.h"

#include <memory>
#include <functional>
#include <cstddef>

namespace io {

class TlsTcpServer : public Removable,
                     public UserDataHolder {
public:
    using NewConnectionCallback = std::function<void(TlsTcpConnectedClient&, const Error&)>;
    using DataReceivedCallback = std::function<void(TlsTcpConnectedClient&, const DataChunk&, const Error&)>;
    using CloseConnectionCallback = std::function<void(TlsTcpConnectedClient&, const Error&)>;

    using CloseServerCallback = std::function<void(TlsTcpServer&, const Error&)>;
    using ShutdownServerCallback = std::function<void(TlsTcpServer&, const Error&)>;

    IO_FORBID_COPY(TlsTcpServer);
    IO_FORBID_MOVE(TlsTcpServer);

    IO_DLL_PUBLIC TlsTcpServer(EventLoop& loop,
                               const Path& certificate_path,
                               const Path& private_key_path,
                               TlsVersionRange version_range = DEFAULT_TLS_VERSION_RANGE);

    IO_DLL_PUBLIC
    Error listen(const Endpoint endpoint,
                 NewConnectionCallback new_connection_callback,
                 DataReceivedCallback data_receive_callback,
                 CloseConnectionCallback close_connection_callback,
                 int backlog_size = 128);

    IO_DLL_PUBLIC
    Error listen(const Endpoint endpoint,
                 NewConnectionCallback new_connection_callback,
                 DataReceivedCallback data_receive_callback,
                 int backlog_size = 128);

    IO_DLL_PUBLIC
    Error listen(const Endpoint endpoint,
                 DataReceivedCallback data_receive_callback,
                 int backlog_size = 128);

    IO_DLL_PUBLIC void shutdown(ShutdownServerCallback shutdown_callback = nullptr);
    IO_DLL_PUBLIC void close(CloseServerCallback close_callback = nullptr);

    IO_DLL_PUBLIC std::size_t connected_clients_count() const;

    IO_DLL_PUBLIC TlsVersionRange version_range() const;

protected:
    IO_DLL_PUBLIC ~TlsTcpServer();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

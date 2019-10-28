#pragma once

#include "Disposable.h"
#include "Error.h"
#include "EventLoop.h"
#include "Export.h"
#include "Path.h"
#include "TlsTcpConnectedClient.h"

#include <memory>
#include <functional>
#include <cstddef>

namespace io {

class TlsTcpServer : public Disposable,
                     public UserDataHolder {
public:
    using NewConnectionCallback = std::function<void(TlsTcpServer&, TlsTcpConnectedClient&)>;
    using DataReceivedCallback = std::function<void(TlsTcpServer&, TlsTcpConnectedClient&, const char*, std::size_t)>;
    using CloseConnectionCallback = std::function<void(TlsTcpServer&, TlsTcpConnectedClient&, const Error&)>;

    IO_DLL_PUBLIC TlsTcpServer(EventLoop& loop,
                               const Path& certificate_path,
                               const Path& private_key_path);

    // TODO: some sort of macro here???
    TlsTcpServer(const TlsTcpServer& other) = delete;
    TlsTcpServer& operator=(const TlsTcpServer& other) = delete;

    TlsTcpServer(TlsTcpServer&& other) = default;
    TlsTcpServer& operator=(TlsTcpServer&& other) = delete; // default

    IO_DLL_PUBLIC
    Error listen(const std::string& ip_addr_str,
                 std::uint16_t port,
                 NewConnectionCallback new_connection_callback,
                 DataReceivedCallback data_receive_callback,
                 int backlog_size = 128);

    IO_DLL_PUBLIC
    Error listen(const std::string& ip_addr_str,
                 std::uint16_t port,
                 DataReceivedCallback data_receive_callback,
                 int backlog_size = 128);

    IO_DLL_PUBLIC void shutdown();
    IO_DLL_PUBLIC void close();

    IO_DLL_PUBLIC std::size_t connected_clients_count() const;

    // TODO: remove
    IO_DLL_PUBLIC ~TlsTcpServer();
protected:
    //~TlsTcpServer(); // TODO: fixme

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

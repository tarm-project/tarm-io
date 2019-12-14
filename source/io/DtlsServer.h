#pragma once

#include "Disposable.h"
#include "Error.h"
#include "EventLoop.h"
#include "Export.h"
#include "Path.h"
#include "DtlsConnectedClient.h"

#include <memory>
#include <functional>
#include <cstddef>

namespace io {

class DtlsServer : public Disposable,
                   public UserDataHolder {
public:
    // TODO: error should be in every callback????
    using NewConnectionCallback = std::function<void(DtlsServer&, DtlsConnectedClient&)>;
    using DataReceivedCallback = std::function<void(DtlsServer&, DtlsConnectedClient&, const DataChunk&)>;
    using CloseConnectionCallback = std::function<void(DtlsServer&, DtlsConnectedClient&, const Error&)>;

    IO_DLL_PUBLIC DtlsServer(EventLoop& loop,
                             const Path& certificate_path,
                             const Path& private_key_path);

    // TODO: some sort of macro here???
    DtlsServer(const DtlsServer& other) = delete;
    DtlsServer& operator=(const DtlsServer& other) = delete;

    DtlsServer(DtlsServer&& other) = default;
    DtlsServer& operator=(DtlsServer&& other) = delete; // default

    IO_DLL_PUBLIC
    Error listen(const std::string& ip_addr_str,
                 std::uint16_t port,
                 NewConnectionCallback new_connection_callback,
                 DataReceivedCallback data_receive_callback);

    IO_DLL_PUBLIC
    Error listen(const std::string& ip_addr_str,
                 std::uint16_t port,
                 DataReceivedCallback data_receive_callback);

    IO_DLL_PUBLIC void shutdown();
    IO_DLL_PUBLIC void close();

    IO_DLL_PUBLIC std::size_t connected_clients_count() const;

    // TODO: remove
    IO_DLL_PUBLIC ~DtlsServer();
protected:
    //~DtlsServer(); // TODO: fixme

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

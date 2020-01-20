#pragma once

#include "CommonMacros.h"
#include "DtlsConnectedClient.h"
#include "DtlsVersion.h"
#include "Error.h"
#include "EventLoop.h"
#include "Export.h"
#include "Path.h"
#include "Removable.h"

#include <memory>
#include <functional>
#include <cstddef>

namespace io {

class DtlsServer : public Removable,
                   public UserDataHolder {
public:
    using NewConnectionCallback = std::function<void(DtlsConnectedClient&, const Error&)>;
    using DataReceivedCallback = std::function<void(DtlsConnectedClient&, const DataChunk&, const Error&)>;
    using CloseConnectionCallback = std::function<void(DtlsConnectedClient&, const Error&)>;

    IO_FORBID_COPY(DtlsServer);
    IO_DECLARE_DLL_PUBLIC_MOVE(DtlsServer);

    IO_DLL_PUBLIC DtlsServer(EventLoop& loop,
                             const Path& certificate_path,
                             const Path& private_key_path,
                             DtlsVersionRange version_range = DEFAULT_DTLS_VERSION_RANGE);

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

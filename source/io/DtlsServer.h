#pragma once

#include "CommonMacros.h"
#include "DtlsConnectedClient.h"
#include "DtlsVersion.h"
#include "Endpoint.h"
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
    friend class DtlsConnectedClient;

    using NewConnectionCallback = std::function<void(DtlsConnectedClient&, const Error&)>;
    using DataReceivedCallback = std::function<void(DtlsConnectedClient&, const DataChunk&, const Error&)>;
    using CloseConnectionCallback = std::function<void(DtlsConnectedClient&, const Error&)>;

    using CloseServerCallback = std::function<void(DtlsServer&, const Error&)>;

    static const std::size_t DEFAULT_TIMEOUT_MS = 1000;

    IO_FORBID_COPY(DtlsServer);
    IO_FORBID_MOVE(DtlsServer);

    IO_DLL_PUBLIC DtlsServer(EventLoop& loop,
                             const Path& certificate_path,
                             const Path& private_key_path,
                             DtlsVersionRange version_range = DEFAULT_DTLS_VERSION_RANGE);

    IO_DLL_PUBLIC
    Error listen(const Endpoint& endpoint,
                 DataReceivedCallback data_receive_callback);

    IO_DLL_PUBLIC
    Error listen(const Endpoint& endpoint,
                 NewConnectionCallback new_connection_callback,
                 DataReceivedCallback data_receive_callback);

   IO_DLL_PUBLIC
   Error listen(const Endpoint& endpoint,
                NewConnectionCallback new_connection_callback,
                DataReceivedCallback data_receive_callback,
                std::size_t timeout_ms,
                CloseConnectionCallback close_callback);

    IO_DLL_PUBLIC void close(CloseServerCallback callback = nullptr);

    IO_DLL_PUBLIC std::size_t connected_clients_count() const;

    IO_DLL_PUBLIC void schedule_removal() override;

protected:
    IO_DLL_PUBLIC ~DtlsServer();

private:
    void remove_client(DtlsConnectedClient& client);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

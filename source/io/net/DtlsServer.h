/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "net/DtlsConnectedClient.h"
#include "net/DtlsVersion.h"
#include "net/Endpoint.h"
#include "Error.h"
#include "EventLoop.h"
#include "Export.h"
#include "Removable.h"
#include "fs/Path.h"

#include <memory>
#include <functional>
#include <cstddef>

namespace tarm {
namespace io {
namespace net {

class DtlsServer : public Removable,
                   public UserDataHolder {
public:
    friend class ::tarm::io::Allocator;

    using AssociatedClientType = DtlsConnectedClient;

    friend class DtlsConnectedClient;

    using NewConnectionCallback = std::function<void(DtlsConnectedClient&, const Error&)>;
    using DataReceivedCallback = std::function<void(DtlsConnectedClient&, const DataChunk&, const Error&)>;
    using CloseConnectionCallback = std::function<void(DtlsConnectedClient&, const Error&)>;

    using CloseServerCallback = std::function<void(DtlsServer&, const Error&)>;

    static const std::size_t DEFAULT_TIMEOUT_MS = 1000;

    TARM_IO_FORBID_COPY(DtlsServer);
    TARM_IO_FORBID_MOVE(DtlsServer);

    TARM_IO_DLL_PUBLIC
    Error listen(const Endpoint& endpoint,
                 const DataReceivedCallback& data_receive_callback);

    TARM_IO_DLL_PUBLIC
    Error listen(const Endpoint& endpoint,
                 const NewConnectionCallback& new_connection_callback,
                 const DataReceivedCallback& data_receive_callback);

   TARM_IO_DLL_PUBLIC
   Error listen(const Endpoint& endpoint,
                const NewConnectionCallback& new_connection_callback,
                const DataReceivedCallback& data_receive_callback,
                std::size_t timeout_ms,
                const CloseConnectionCallback& close_callback);

    TARM_IO_DLL_PUBLIC void close(const CloseServerCallback& callback = nullptr);

    TARM_IO_DLL_PUBLIC std::size_t connected_clients_count() const;

    TARM_IO_DLL_PUBLIC void schedule_removal() override;

protected:
    TARM_IO_DLL_PUBLIC DtlsServer(AllocationContext& context,
                                  const fs::Path& certificate_path,
                                  const fs::Path& private_key_path,
                                  DtlsVersionRange version_range);

    TARM_IO_DLL_PUBLIC DtlsServer(AllocationContext& context,
                                  const fs::Path& certificate_path,
                                  const fs::Path& private_key_path);

    TARM_IO_DLL_PUBLIC ~DtlsServer();

private:
    void remove_client(DtlsConnectedClient& client);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace net
} // namespace io
} // namespace tarm

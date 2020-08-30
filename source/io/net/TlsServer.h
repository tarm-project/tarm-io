/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "net/Endpoint.h"
#include "Error.h"
#include "EventLoop.h"
#include "Export.h"
#include "Removable.h"
#include "net/TlsConnectedClient.h"
#include "fs/Path.h"

#include <memory>
#include <functional>
#include <cstddef>

namespace tarm {
namespace io {
namespace net {

class TlsServer : public Removable,
                     public UserDataHolder {
public:
    using NewConnectionCallback = std::function<void(TlsConnectedClient&, const Error&)>;
    using DataReceivedCallback = std::function<void(TlsConnectedClient&, const DataChunk&, const Error&)>;
    using CloseConnectionCallback = std::function<void(TlsConnectedClient&, const Error&)>;

    using CloseServerCallback = std::function<void(TlsServer&, const Error&)>;
    using ShutdownServerCallback = std::function<void(TlsServer&, const Error&)>;

    TARM_IO_FORBID_COPY(TlsServer);
    TARM_IO_FORBID_MOVE(TlsServer);

    TARM_IO_DLL_PUBLIC TlsServer(EventLoop& loop,
                                 const fs::Path& certificate_path,
                                 const fs::Path& private_key_path,
                                 TlsVersionRange version_range = DEFAULT_TLS_VERSION_RANGE);

    TARM_IO_DLL_PUBLIC void schedule_removal() override;

    TARM_IO_DLL_PUBLIC
    Error listen(const Endpoint endpoint,
                 const NewConnectionCallback& new_connection_callback,
                 const DataReceivedCallback& data_receive_callback,
                 const CloseConnectionCallback& close_connection_callback,
                 int backlog_size = 128);

    TARM_IO_DLL_PUBLIC
    Error listen(const Endpoint endpoint,
                 const NewConnectionCallback& new_connection_callback,
                 const DataReceivedCallback& data_receive_callback,
                 int backlog_size = 128);

    TARM_IO_DLL_PUBLIC
    Error listen(const Endpoint endpoint,
                 const DataReceivedCallback& data_receive_callback,
                 int backlog_size = 128);

    TARM_IO_DLL_PUBLIC void shutdown(const ShutdownServerCallback& shutdown_callback = nullptr);
    TARM_IO_DLL_PUBLIC void close(const CloseServerCallback& close_callback = nullptr);

    TARM_IO_DLL_PUBLIC std::size_t connected_clients_count() const;

    TARM_IO_DLL_PUBLIC TlsVersionRange version_range() const;

protected:
    TARM_IO_DLL_PUBLIC ~TlsServer();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace net
} // namespace io
} // namespace tarm

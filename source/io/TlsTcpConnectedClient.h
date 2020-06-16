/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "EventLoop.h"
#include "Error.h"
#include "Export.h"
#include "Forward.h"
#include "Removable.h"
#include "TlsVersion.h"

#include <memory>

namespace tarm {
namespace io {

class TlsTcpConnectedClient : protected Removable,
                              public UserDataHolder {
public:
    friend class TlsTcpServer;

    using UnderlyingClientType = TcpConnectedClient;

    using DataReceiveCallback = std::function<void(TlsTcpConnectedClient&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(TlsTcpConnectedClient&, const Error&)>;
    using EndSendCallback = std::function<void(TlsTcpConnectedClient&, const Error&)>;

    using NewConnectionCallback = std::function<void(TlsTcpConnectedClient&, const Error&)>;

    IO_FORBID_COPY(TlsTcpConnectedClient);
    IO_FORBID_MOVE(TlsTcpConnectedClient);

    IO_DLL_PUBLIC void close();
    IO_DLL_PUBLIC void shutdown();
    IO_DLL_PUBLIC bool is_open() const;

    // TODO: char* and std::string&& send overloads
    IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(const std::string& message, EndSendCallback callback = nullptr);

    IO_DLL_PUBLIC TlsTcpServer& server();
    IO_DLL_PUBLIC const TlsTcpServer& server() const;

    IO_DLL_PUBLIC TlsVersion negotiated_tls_version() const;

protected:
    ~TlsTcpConnectedClient();

private:
    TlsTcpConnectedClient(EventLoop& loop,
                          TlsTcpServer& tls_server,
                          NewConnectionCallback new_connection_callback,
                          TcpConnectedClient& tcp_client,
                          void* context);

    void set_data_receive_callback(DataReceiveCallback callback);
    void on_data_receive(const char* buf, std::size_t size);
    Error init_ssl();

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
} // namespace tarm


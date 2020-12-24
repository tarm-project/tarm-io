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
#include "net/TlsVersion.h"

#include <memory>

namespace tarm {
namespace io {
namespace net {

class TlsConnectedClient : protected Removable,
                           public UserDataHolder {
public:
    friend class TlsServer;

    using UnderlyingClientType = TcpConnectedClient;

    using DataReceiveCallback = std::function<void(TlsConnectedClient&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(TlsConnectedClient&, const Error&)>;
    using EndSendCallback = std::function<void(TlsConnectedClient&, const Error&)>;

    using NewConnectionCallback = std::function<void(TlsConnectedClient&, const Error&)>;

    TARM_IO_FORBID_COPY(TlsConnectedClient);
    TARM_IO_FORBID_MOVE(TlsConnectedClient);

    TARM_IO_DLL_PUBLIC void close();
    TARM_IO_DLL_PUBLIC void shutdown();
    TARM_IO_DLL_PUBLIC bool is_open() const;

    TARM_IO_DLL_PUBLIC void copy_and_send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(const std::string& message, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(std::string&& message, const EndSendCallback& callback = nullptr);

    TARM_IO_DLL_PUBLIC TlsServer& server();
    TARM_IO_DLL_PUBLIC const TlsServer& server() const;

    TARM_IO_DLL_PUBLIC TlsVersion negotiated_tls_version() const;

protected:
    ~TlsConnectedClient();

private:
    TlsConnectedClient(EventLoop& loop,
                       TlsServer& tls_server,
                       const NewConnectionCallback& new_connection_callback,
                       TcpConnectedClient& tcp_client,
                       void* context);

    void set_data_receive_callback(const DataReceiveCallback& callback);
    void on_data_receive(const char* buf, std::size_t size, const Error& error);
    Error init_ssl();

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace net
} // namespace io
} // namespace tarm


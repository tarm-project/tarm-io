/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "net/DtlsVersion.h"
#include "net/Endpoint.h"
#include "EventLoop.h"
#include "Error.h"
#include "Export.h"
#include "Forward.h"
#include "Removable.h"

#include <memory>

namespace tarm {
namespace io {
namespace net {

class DtlsConnectedClient : protected Removable,
                            public UserDataHolder {
public:
    friend class DtlsServer;

    using UnderlyingClientType = UdpPeer;

    using NewConnectionCallback = std::function<void(DtlsConnectedClient&, const Error&)>;
    using DataReceiveCallback = std::function<void(DtlsConnectedClient&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(DtlsConnectedClient&, const Error&)>;
    using EndSendCallback = std::function<void(DtlsConnectedClient&, const Error&)>;

    TARM_IO_FORBID_COPY(DtlsConnectedClient);
    TARM_IO_FORBID_MOVE(DtlsConnectedClient);

    TARM_IO_DLL_PUBLIC void close();
    TARM_IO_DLL_PUBLIC bool is_open() const;

    TARM_IO_DLL_PUBLIC const Endpoint& endpoint() const;

    TARM_IO_DLL_PUBLIC void send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(const std::string& message, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(std::string&& message, const EndSendCallback& callback = nullptr);

    TARM_IO_DLL_PUBLIC DtlsVersion negotiated_dtls_version() const;

    TARM_IO_DLL_PUBLIC DtlsServer& server();
    TARM_IO_DLL_PUBLIC const DtlsServer& server() const;

protected:
    ~DtlsConnectedClient();

private:
    DtlsConnectedClient(EventLoop& loop,
                        DtlsServer& dtls_server,
                        UdpPeer& udp_peer);

    DtlsConnectedClient(EventLoop& loop,
                        DtlsServer& dtls_server,
                        const NewConnectionCallback& new_connection_callback,
                        const CloseCallback& close_callback,
                        UdpPeer& udp_peer,
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



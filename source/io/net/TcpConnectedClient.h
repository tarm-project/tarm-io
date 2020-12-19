/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "net/Endpoint.h"
#include "EventLoop.h"
#include "Export.h"
#include "Error.h"
#include "Forward.h"
#include "Removable.h"
#include "UserDataHolder.h"

#include <memory>

namespace tarm {
namespace io {
namespace net {

class TcpConnectedClient : protected Removable,
                           public UserDataHolder {
public:
    friend class TcpServer;

    using CloseCallback = std::function<void(TcpConnectedClient&, const Error&)>;
    using EndSendCallback = std::function<void(TcpConnectedClient&, const Error&)>;
    using DataReceiveCallback = std::function<void(TcpConnectedClient&, const DataChunk&, const Error&)>;

    TARM_IO_FORBID_COPY(TcpConnectedClient);
    TARM_IO_FORBID_MOVE(TcpConnectedClient);

    TARM_IO_DLL_PUBLIC const Endpoint& endpoint() const;

    TARM_IO_DLL_PUBLIC void close();
    TARM_IO_DLL_PUBLIC void shutdown();
    TARM_IO_DLL_PUBLIC bool is_open() const;

    // DOC: in case of char* caller is responsible for buffer to be alive until EndSendCallback is called
    TARM_IO_DLL_PUBLIC void copy_and_send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(const std::string& message, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(std::string&& message, const EndSendCallback& callback = nullptr);

    TARM_IO_DLL_PUBLIC std::size_t pending_send_requesets() const;

    TARM_IO_DLL_PUBLIC void delay_send(bool enabled);
    TARM_IO_DLL_PUBLIC bool is_delay_send() const;

    // Used to terminate connection immediately and notify other side about
    TARM_IO_DLL_PUBLIC void close_with_reset();

    TARM_IO_DLL_PUBLIC TcpServer& server();
    TARM_IO_DLL_PUBLIC const TcpServer& server() const;

protected:
    TARM_IO_DLL_PUBLIC TcpConnectedClient(EventLoop& loop, TcpServer& server, const CloseCallback& cloase_callback);
    TARM_IO_DLL_PUBLIC ~TcpConnectedClient();

    TARM_IO_DLL_PUBLIC void schedule_removal() override;

private:
    Error init_stream();
    void start_read(const DataReceiveCallback& data_receive_callback);
    void* tcp_client_stream();

    void set_endpoint(const Endpoint& endpoint);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace net
} // namespace io
} // namespace tarm

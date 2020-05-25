/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "Endpoint.h"
#include "EventLoop.h"
#include "Export.h"
#include "Error.h"
#include "Forward.h"
#include "Removable.h"
#include "UserDataHolder.h"

#include <memory>

namespace tarm {
namespace io {

class TcpConnectedClient : protected Removable,
                           public UserDataHolder {
public:
    friend class TcpServer;

    using CloseCallback = std::function<void(TcpConnectedClient&, const Error&)>;
    using EndSendCallback = std::function<void(TcpConnectedClient&, const Error&)>;
    using DataReceiveCallback = std::function<void(TcpConnectedClient&, const DataChunk&, const Error&)>;

    IO_FORBID_COPY(TcpConnectedClient);
    IO_FORBID_MOVE(TcpConnectedClient);

    IO_DLL_PUBLIC const Endpoint& endpoint() const;

    IO_DLL_PUBLIC void close();
    IO_DLL_PUBLIC void shutdown();
    IO_DLL_PUBLIC bool is_open() const;

    // global TODO: add send_data which accepts char* and size
    IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(const std::string& message, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(std::string&& message, EndSendCallback callback = nullptr);

    // TODO: rename as pending_send_requesets??? Because name is inconsistent.
    IO_DLL_PUBLIC std::size_t pending_write_requesets() const;

    IO_DLL_PUBLIC void delay_send(bool enabled);
    IO_DLL_PUBLIC bool is_delay_send() const;

    IO_DLL_PUBLIC TcpServer& server();
    IO_DLL_PUBLIC const TcpServer& server() const;

protected:
    IO_DLL_PUBLIC TcpConnectedClient(EventLoop& loop, TcpServer& server, CloseCallback cloase_callback);
    IO_DLL_PUBLIC ~TcpConnectedClient();

    IO_DLL_PUBLIC void schedule_removal() override;

private:
    Error init_stream();
    void start_read(DataReceiveCallback data_receive_callback);
    void* tcp_client_stream();

    void set_endpoint(const Endpoint& endpoint);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
} // namespace tarm

/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "net/Endpoint.h"
#include "EventLoop.h"
#include "Export.h"
#include "DataChunk.h"
#include "Removable.h"
#include "UserDataHolder.h"
#include "Error.h"

#include <memory>

namespace tarm {
namespace io {
namespace net {

class TcpClient : public Removable,
                  public UserDataHolder {
public:
    using ConnectCallback = std::function<void(TcpClient&, const Error&)>;
    using DataReceiveCallback = std::function<void(TcpClient&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(TcpClient&, const Error&)>;
    using EndSendCallback = std::function<void(TcpClient&, const Error&)>;

    TARM_IO_FORBID_COPY(TcpClient);
    TARM_IO_FORBID_MOVE(TcpClient);

    TARM_IO_DLL_PUBLIC TcpClient(EventLoop& loop);

    TARM_IO_DLL_PUBLIC void schedule_removal() override;

    TARM_IO_DLL_PUBLIC const Endpoint& endpoint() const;

    TARM_IO_DLL_PUBLIC
    void connect(const Endpoint& endpoint,
                 const ConnectCallback& connect_callback,
                 const DataReceiveCallback& receive_callback = nullptr,
                 const CloseCallback& close_callback = nullptr);
    TARM_IO_DLL_PUBLIC void close();

    // Used to terminate connection immediately and notify other side about
    TARM_IO_DLL_PUBLIC void close_with_reset();

    TARM_IO_DLL_PUBLIC bool is_open() const;

    // DOC: successful EndSendCallback does not mean that client has received that message successfully.
    //      It only means that message has finished transfering to operating system internals which are responsible
    //      for networking operations.
    //      For guarantee of messagee delivery you may implement application level acknowledgement message.
    //      This is true even for TCP because when pacckets are transfered to OS buffer, other side may crash or
    //      connectivity lost. So data will not be received.
    //      Notice that different operation systems behave in their own way. Windows will copy to kernel space
    //      all available data, even if it is gigabytes in size. Linux and Mac will call EndSendCallback when last
    //      chunk of data (which is proportional of send buffer size) is transfered to the kernel.
    TARM_IO_DLL_PUBLIC void send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(const std::string& message, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(std::string&& message, const EndSendCallback& callback = nullptr);

    TARM_IO_DLL_PUBLIC std::size_t pending_send_requesets() const;

    TARM_IO_DLL_PUBLIC void shutdown();

    TARM_IO_DLL_PUBLIC void delay_send(bool enabled);
    TARM_IO_DLL_PUBLIC bool is_delay_send() const;

protected:
    TARM_IO_DLL_PUBLIC ~TcpClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

using TcpClientPtr = std::unique_ptr<TcpClient>;

} // namespace net
} // namespace io
} // namespace tarm

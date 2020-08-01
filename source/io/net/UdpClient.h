/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "BufferSizeResult.h"
#include "CommonMacros.h"
#include "DataChunk.h"
#include "net/Endpoint.h"
#include "Error.h"
#include "EventLoop.h"
#include "Export.h"
#include "Forward.h"
#include "Removable.h"
#include "UserDataHolder.h"

#include <memory>
#include <functional>

namespace tarm {
namespace io {
namespace net {

class UdpClient : public Removable,
                  public UserDataHolder {
public:
    using DestinationSetCallback = std::function<void(UdpClient&, const Error&)>;
    using DataReceivedCallback = std::function<void(UdpClient&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(UdpClient&, const Error&)>;
    using EndSendCallback = std::function<void(UdpClient&, const Error&)>;

    IO_FORBID_COPY(UdpClient);
    IO_FORBID_MOVE(UdpClient);

    IO_DLL_PUBLIC UdpClient(EventLoop& loop);

    // Analog of connect for TCP
    IO_DLL_PUBLIC void set_destination(const Endpoint& endpoint,
                                       const DestinationSetCallback& destination_set_callback,
                                       const DataReceivedCallback& receive_callback = nullptr);

    IO_DLL_PUBLIC void set_destination(const Endpoint& endpoint,
                                       const DestinationSetCallback& destination_set_callback,
                                       const DataReceivedCallback& receive_callback,
                                       std::size_t timeout_ms,
                                       const CloseCallback& close_callback);

    IO_DLL_PUBLIC const Endpoint& endpoint() const;
    IO_DLL_PUBLIC std::uint16_t bound_port() const; // Returns 0 on error

    IO_DLL_PUBLIC  bool is_open() const;

    IO_DLL_PUBLIC BufferSizeResult receive_buffer_size() const;
    IO_DLL_PUBLIC BufferSizeResult send_buffer_size() const;
    IO_DLL_PUBLIC Error set_receive_buffer_size(std::size_t size);
    IO_DLL_PUBLIC Error set_send_buffer_size(std::size_t size);

    IO_DLL_PUBLIC void send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr);
    IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback = nullptr);
    IO_DLL_PUBLIC void send_data(const std::string& message, const EndSendCallback& callback = nullptr);
    IO_DLL_PUBLIC void send_data(std::string&& message, const EndSendCallback& callback = nullptr);

    IO_DLL_PUBLIC void schedule_removal() override;

    IO_DLL_PUBLIC void close();

protected:
    IO_DLL_PUBLIC ~UdpClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace net
} // namespace io
} // namespace tarm

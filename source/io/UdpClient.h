#pragma once

#include "EventLoop.h"
#include "Export.h"
#include "Forward.h"
#include "DataChunk.h"
#include "Disposable.h"
#include "Error.h"
#include "UserDataHolder.h"

#include <memory>
#include <functional>

namespace io {

class UdpClient : public Disposable,
                  public UserDataHolder {
public:
    using EndSendCallback = std::function<void(UdpClient&, const Error&)>;
    using DataReceivedCallback = std::function<void(UdpClient&, const DataChunk&, const Error&)>;

    IO_DLL_PUBLIC UdpClient(EventLoop& loop);
    IO_DLL_PUBLIC UdpClient(EventLoop& loop, std::uint32_t dest_host, std::uint16_t dest_port);
    IO_DLL_PUBLIC UdpClient(EventLoop& loop, DataReceivedCallback receive_callback);
    IO_DLL_PUBLIC UdpClient(EventLoop& loop, std::uint32_t dest_host, std::uint16_t dest_port, DataReceivedCallback receive_callback);

    IO_DLL_PUBLIC void set_destination(std::uint32_t host, std::uint16_t port);

    // TODO: implement???
    //void send_data(const DataChunk& data_chunk, std::uint32_t host, std::uint16_t port, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(const std::string& message, EndSendCallback callback = nullptr);

    IO_DLL_PUBLIC void schedule_removal() override;

protected:
    IO_DLL_PUBLIC ~UdpClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

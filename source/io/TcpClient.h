#pragma once

#include "CommonMacros.h"
#include "Endpoint.h"
#include "EventLoop.h"
#include "Export.h"
#include "DataChunk.h"
#include "Removable.h"
#include "UserDataHolder.h"
#include "Error.h"

#include <memory>

namespace io {

class TcpClient : public Removable,
                  public UserDataHolder {
public:
    using ConnectCallback = std::function<void(TcpClient&, const Error&)>;
    using CloseCallback = std::function<void(TcpClient&, const Error&)>;
    using EndSendCallback = std::function<void(TcpClient&, const Error&)>;
    using DataReceiveCallback = std::function<void(TcpClient&, const DataChunk&, const Error&)>;

    IO_FORBID_COPY(TcpClient);
    IO_FORBID_MOVE(TcpClient);

    IO_DLL_PUBLIC TcpClient(EventLoop& loop);

    IO_DLL_PUBLIC void schedule_removal() override;

    IO_DLL_PUBLIC std::uint32_t ipv4_addr() const;
    IO_DLL_PUBLIC std::uint16_t port() const;

    IO_DLL_PUBLIC
    void connect(const Endpoint& endpoint,
                 ConnectCallback connect_callback,
                 DataReceiveCallback receive_callback = nullptr,
                 CloseCallback close_callback = nullptr);
    IO_DLL_PUBLIC void close();

    IO_DLL_PUBLIC bool is_open() const;

    IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(const std::string& message, EndSendCallback callback = nullptr);

    IO_DLL_PUBLIC std::size_t pending_write_requesets() const;

    IO_DLL_PUBLIC void shutdown(CloseCallback callback = nullptr);

    IO_DLL_PUBLIC void delay_send(bool enabled);
    IO_DLL_PUBLIC bool is_delay_send() const;

protected:
    IO_DLL_PUBLIC ~TcpClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

using TcpClientPtr = std::unique_ptr<TcpClient>;

} // namespace io

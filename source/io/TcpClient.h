#pragma once

#include "EventLoop.h"
#include "Export.h"
#include "Disposable.h"
#include "UserDataHolder.h"
#include "Status.h"

#include <memory>

namespace io {

class TcpClient : public Disposable,
                  public UserDataHolder {
public:
    using ConnectCallback = std::function<void(TcpClient&, const Error&)>;
    using CloseCallback = std::function<void(TcpClient&, const Error&)>;
    using EndSendCallback = std::function<void(TcpClient&, const Error&)>;
    using DataReceiveCallback = std::function<void(TcpClient&, const char*, size_t)>;

    IO_DLL_PUBLIC TcpClient(EventLoop& loop);

    IO_DLL_PUBLIC void schedule_removal() override;

    IO_DLL_PUBLIC std::uint32_t ipv4_addr() const;
    IO_DLL_PUBLIC std::uint16_t port() const;

    IO_DLL_PUBLIC
    void connect(const std::string& address,
                 std::uint16_t port,
                 ConnectCallback connect_callback,
                 DataReceiveCallback receive_callback = nullptr,
                 CloseCallback close_callback = nullptr);
    IO_DLL_PUBLIC void close();

    IO_DLL_PUBLIC bool is_open() const;

    IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(const std::string& message, EndSendCallback callback = nullptr);

    IO_DLL_PUBLIC void set_close_callback(CloseCallback callback);

    IO_DLL_PUBLIC std::size_t pending_write_requesets() const;

    IO_DLL_PUBLIC void shutdown(CloseCallback callback = nullptr);

protected:
    IO_DLL_PUBLIC ~TcpClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

using TcpClientPtr = std::unique_ptr<TcpClient>;

} // namespace io

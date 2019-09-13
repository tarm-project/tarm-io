#pragma once

#include "Disposable.h"
#include "EventLoop.h"
#include "Error.h"
#include "Export.h"

#include <memory>

namespace io {

class TlsTcpClient : public Disposable {
public:
    using ConnectCallback = std::function<void(TlsTcpClient&, const Error&)>;
    using CloseCallback = std::function<void(TlsTcpClient&, const Error&)>;
    using EndSendCallback = std::function<void(TlsTcpClient&, const Error&)>;
    using DataReceiveCallback = std::function<void(TlsTcpClient&, const char*, size_t)>;

    IO_DLL_PUBLIC TlsTcpClient(EventLoop& loop);

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

protected:
    IO_DLL_PUBLIC ~TlsTcpClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
#pragma once

#include "Common.h"
#include "Disposable.h"
#include "EventLoop.h"
#include "Status.h"

#include <memory>

namespace io {

class TcpClient : public Disposable {
public:
    using ConnectCallback = std::function<void(TcpClient&, const Status&)>;
    using CloseCallback = std::function<void(TcpClient&, const Status&)>;
    using EndSendCallback = std::function<void(TcpClient&)>;
    using DataReceiveCallback = std::function<void(TcpClient&, const char*, size_t)>;

    TcpClient(EventLoop& loop);

    void schedule_removal() override;

    std::uint32_t ipv4_addr() const;
    std::uint16_t port() const;

    void connect(const std::string& address,
                 std::uint16_t port,
                 ConnectCallback connect_callback,
                 DataReceiveCallback receive_callback,
                 CloseCallback close_callback = nullptr);
    void close();

    bool is_open() const;

    void set_ipv4_addr(std::uint32_t value);
    void set_port(std::uint16_t value);

    void send_data(std::shared_ptr<const char> buffer, std::size_t size, EndSendCallback callback = nullptr);
    void send_data(const std::string& message, EndSendCallback callback = nullptr);

    void set_close_callback(CloseCallback callback);

    std::size_t pending_write_requesets() const;

    void shutdown();

    void set_user_data(void* data);
    void* user_data();

protected:
    ~TcpClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

using TcpClientPtr = std::unique_ptr<TcpClient>;

} // namespace io

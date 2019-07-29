#pragma once

#include "Common.h"
#include "Disposable.h"
#include "EventLoop.h"
#include "Status.h"
#include "UserDataHolder.h"

#include <memory>

namespace io {

// TODO: move to fwd header
class TcpServer;

class TcpConnectedClient : protected Disposable,
                           public UserDataHolder {
public:
    friend class TcpServer;

    using CloseCallback = std::function<void(TcpConnectedClient&, const Status&)>;
    using EndSendCallback = std::function<void(TcpConnectedClient&, const Status&)>;
    using DataReceiveCallback = std::function<void(TcpServer&, TcpConnectedClient&, const char*, std::size_t)>;

    std::uint32_t ipv4_addr() const;
    std::uint16_t port() const;

    void close();
    void shutdown();
    bool is_open() const;

    void set_close_callback(CloseCallback callback);

    void send_data(std::shared_ptr<const char> buffer, std::size_t size, EndSendCallback callback = nullptr);
    void send_data(const std::string& message, EndSendCallback callback = nullptr);
    std::size_t pending_write_requesets() const;

protected:
    TcpConnectedClient(EventLoop& loop, TcpServer& server);
    ~TcpConnectedClient();

    void schedule_removal() override;

private:
    void start_read(DataReceiveCallback data_receive_callback);
    void* tcp_client_stream();

    void set_ipv4_addr(std::uint32_t value);
    void set_port(std::uint16_t value);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

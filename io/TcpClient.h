#pragma once

#include "Common.h"
#include "Disposable.h"
#include "EventLoop.h"

#include <memory>

namespace io {

// TODO: move to fwd header
class TcpServer;

class TcpClient : public Disposable {
public:
    friend class TcpServer;

    using CloseCallback = std::function<void(TcpClient&)>;
    using EndSendCallback = std::function<void(TcpClient&)>;

    TcpClient(EventLoop& loop, TcpServer& server);

    std::uint32_t ipv4_addr() const;
    std::uint16_t port() const;

    void set_ipv4_addr(std::uint32_t value);
    void set_port(std::uint16_t value);

    void send_data(const char* buffer, std::size_t size, EndSendCallback callback = nullptr);
    void set_close_callback(CloseCallback callback);
    void close();

    std::size_t pending_write_requesets() const;

    void shutdown();

    void schedule_removal() override;

    static void after_write(uv_write_t* req, int status);

    // statics
    static void on_shutdown(uv_shutdown_t* req, int status);
    static void on_close_cb(uv_handle_t* handle);

protected:
    ~TcpClient();

private:
    const TcpServer& server() const;
    TcpServer& server();

    uv_tcp_t* tcp_client_stream();

    TcpServer* m_server;

    uv_tcp_t m_stream;
    std::uint32_t m_ipv4_addr;
    std::uint16_t m_port;

    std::size_t m_pending_write_requesets = 0;

    CloseCallback m_close_callback = nullptr;
};

using TcpClientPtr = std::unique_ptr<TcpClient>;

} // namespace io
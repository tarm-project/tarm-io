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

    using ConnectCallback = std::function<void(TcpClient& )>;

    using CloseCallback = std::function<void(TcpClient&)>;
    using EndSendCallback = std::function<void(TcpClient&)>;

    TcpClient(EventLoop& loop);
    TcpClient(EventLoop& loop, TcpServer& server);

    void schedule_removal() override;

    std::uint32_t ipv4_addr() const;
    std::uint16_t port() const;

    void connect(const std::string& address, std::uint16_t port, ConnectCallback callback);
    void close();

    bool is_open() const;

    void set_ipv4_addr(std::uint32_t value);
    void set_port(std::uint16_t value);

    void send_data(std::shared_ptr<const char> buffer, std::size_t size, EndSendCallback callback = nullptr);
    void send_data(const std::string& message, EndSendCallback callback = nullptr);

    void set_close_callback(CloseCallback callback);

    std::size_t pending_write_requesets() const;

    void shutdown();

    static void after_write(uv_write_t* req, int status);

    // statics
    static void on_shutdown(uv_shutdown_t* req, int status);
    static void on_close(uv_handle_t* handle);
    static void on_connect(uv_connect_t* req, int status);

protected:
    ~TcpClient();

private:
    const TcpServer& server() const;
    TcpServer& server();

    void init_stream();

    uv_loop_t* m_uv_loop;

    uv_tcp_t* tcp_client_stream();

    TcpServer* m_server = nullptr;

    ConnectCallback m_connect_callback = nullptr;
    uv_connect_t* m_connect_req = nullptr;

    bool m_is_open = false;

    uv_tcp_t* m_tcp_stream = nullptr;
    std::uint32_t m_ipv4_addr;
    std::uint16_t m_port;

    std::size_t m_pending_write_requesets = 0;

    CloseCallback m_close_callback = nullptr;
};

using TcpClientPtr = std::unique_ptr<TcpClient>;

} // namespace io

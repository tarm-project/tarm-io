#pragma once

#include "Common.h"
#include "Disposable.h"
#include "EventLoop.h"
#include "Status.h"

#include <memory>

namespace io {

// TODO: move to fwd header
class TcpServer;

// TODO: make protected inheritance????
class TcpConnectedClient : public Disposable {
public:
    friend class TcpServer;

    using CloseCallback = std::function<void(TcpConnectedClient&, const Status&)>;
    using EndSendCallback = std::function<void(TcpConnectedClient&)>;
    using DataReceiveCallback = std::function<void(TcpConnectedClient&, const char*, size_t)>;

    void schedule_removal() override;

    std::uint32_t ipv4_addr() const;
    std::uint16_t port() const;

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

    // statics
    static void after_write(uv_write_t* req, int status);
    static void alloc_read_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void on_shutdown(uv_shutdown_t* req, int status);
    static void on_close(uv_handle_t* handle);
    static void on_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf);

protected:
    TcpConnectedClient(EventLoop& loop, TcpServer& server);
    ~TcpConnectedClient();

private:
    const TcpServer& server() const;
    TcpServer& server();

    void init_stream();

    uv_loop_t* m_uv_loop;

    uv_tcp_t* tcp_client_stream();

    TcpServer* m_server = nullptr;

    uv_connect_t* m_connect_req = nullptr;

    DataReceiveCallback m_receive_callback = nullptr;

    // TODO: need to ensure that one buffer is enough
    char* m_read_buf = nullptr;
    std::size_t m_read_buf_size = 0;

    bool m_is_open = false;

    uv_tcp_t* m_tcp_stream = nullptr;
    std::uint32_t m_ipv4_addr = 0;
    std::uint16_t m_port = 0;

    std::size_t m_pending_write_requesets = 0;

    CloseCallback m_close_callback = nullptr;

    void* m_user_data = nullptr;
};

using TcpConnectedClientPtr = std::unique_ptr<TcpConnectedClient>;

} // namespace io

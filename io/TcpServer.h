#pragma once

#include "Common.h"
#include "TcpClient.h"

// TODO: forward declare this???
#include "EventLoop.h"

#include <boost/pool/pool.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>

namespace io {

class TcpServer {
public:
    friend class TcpClient;

    const size_t READ_BUFFER_SIZE = 65536;

    // TODO: replace 'int status' with some error
    using NewConnectionCallback = std::function<bool(TcpServer&, TcpClient&)>;
    using DataReceivedCallback = std::function<void(TcpServer&, TcpClient&, const char*, std::size_t)>;

    TcpServer(EventLoop& loop);
    ~TcpServer(); // TODO: need to test if correct shutdown works in case of destruction of the server

    TcpServer(const TcpServer& other) = delete;
    TcpServer& operator=(const TcpServer& other) = delete;

    TcpServer(TcpServer&& other) = default;
    TcpServer& operator=(TcpServer&& other) = delete; // default

    // TODO: need to return some enum (or StatusCode) instead of int
    int bind(const std::string& ip_addr_str, std::uint16_t port);

    // On success, zero is returned
    int listen(NewConnectionCallback new_connection_callback,
               DataReceivedCallback data_receive_callback,
               int backlog_size = 128);

    void shutdown();
    void close();

    std::size_t connected_clients_count() const;

    // statics
    static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

    static void on_new_connection(uv_stream_t* server, int status);
    static void on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf);

    static void on_close(uv_handle_t* handle);
    static void on_shutdown(uv_shutdown_t* req, int status);
private:
    void remove_client_connection(TcpClient* client);

    EventLoop* m_loop;
    uv_loop_t* m_uv_loop;

    uv_tcp_t* m_server_handle = nullptr;

    // TODO: most likely this data member is not need to be data member but some var on stack
    struct sockaddr_in m_unix_addr;

    NewConnectionCallback m_new_connection_callback = nullptr;
    DataReceivedCallback m_data_receive_callback = nullptr;

    // Using such interesting kind of mapping here to be able find connections by raw C pointer
    // and also have benefits of RAII with unique_ptr
    //std::map<uv_tcp_t*, TcpClientPtr> m_client_connections;
    std::set<TcpClient*> m_client_connections;

    // Made as unique_ptr because boost::pool has no move constructor defined
    std::unique_ptr<boost::pool<>> m_pool;
};

} // namespace io

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

class TcpServer : public uv_tcp_t {
public:
    const size_t READ_BUFFER_SIZE = 65536;

    // TODO: replace 'int status' with some error
    using NewConnectionCallback = std::function<bool(TcpServer&, TcpClient&)>;
    using DataReceivedCallback = std::function<void(TcpServer&, TcpClient&, const char*, size_t)>;

    TcpServer(EventLoop& loop);
    ~TcpServer(); // TODO: need to test if correct shutdown works in case of destruction of the server

    TcpServer(const TcpServer& other) = delete;
    TcpServer& operator=(const TcpServer& other) = delete;

    TcpServer(TcpServer&& other) = default;
    TcpServer& operator=(TcpServer&& other) = delete; // default

    int bind(const std::string& ip_addr_str, std::uint16_t port);

    // On success, zero is returned
    int listen(DataReceivedCallback data_receive_callback,
               NewConnectionCallback new_connection_callback = nullptr,
               int backlog_size = 128);

    void shutdown();

    // TODO: implement
    //TcpServer(Loop& loop, const uint32_t ip4_addr_num, std::uint16_t);

    static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

    static void on_new_connection(uv_stream_t* server, int status);
    static void on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf);
private:

    EventLoop* m_loop; // TODO: reuse loop var from handles instead of holding custom one
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
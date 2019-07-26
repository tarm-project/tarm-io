#include "TcpServer.h"

#include "ByteSwap.h"

#include <assert.h>

// TODO: move
#include <iostream>
#include <string>
#include <iomanip>

namespace io {

const size_t TcpServer::READ_BUFFER_SIZE;

class TcpServer::Impl {
public:
    Impl(EventLoop& loop, TcpServer& parent);
    ~Impl();

    int bind(const std::string& ip_addr_str, std::uint16_t port);

    int listen(NewConnectionCallback new_connection_callback,
               DataReceivedCallback data_receive_callback,
               int backlog_size = 128);

    void shutdown();
    void close();

    std::size_t connected_clients_count() const;

    void remove_client_connection(TcpConnectedClient* client);

protected:
    // statics
    static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

    static void on_new_connection(uv_stream_t* server, int status);
    static void on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf);

    static void on_close(uv_handle_t* handle);
    static void on_shutdown(uv_shutdown_t* req, int status);

private:
    TcpServer* m_parent;
    EventLoop* m_loop;
    uv_loop_t* m_uv_loop;

    uv_tcp_t* m_server_handle = nullptr;

    // TODO: most likely this data member is not need to be data member but some var on stack
    struct sockaddr_in m_unix_addr;

    NewConnectionCallback m_new_connection_callback = nullptr;
    DataReceivedCallback m_data_receive_callback = nullptr;

    // Using such interesting kind of mapping here to be able find connections by raw C pointer
    // and also have benefits of RAII with unique_ptr
    //std::map<uv_tcp_t*, TcpConnectedClientPtr> m_client_connections;
    std::set<TcpConnectedClient*> m_client_connections;

    // Made as unique_ptr because boost::pool has no move constructor defined
    std::unique_ptr<boost::pool<>> m_pool;
};

TcpServer::Impl::Impl(EventLoop& loop, TcpServer& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())),
    m_pool(new boost::pool<>(TcpServer::READ_BUFFER_SIZE)) {
}

TcpServer::Impl::~Impl() {
    //shutdown();
}

int TcpServer::Impl::bind(const std::string& ip_addr_str, std::uint16_t port) {
    m_server_handle = new uv_tcp_t;
    auto init_status = uv_tcp_init_ex(m_uv_loop, m_server_handle, AF_INET); // TODO: IPV6 support
    m_server_handle->data = this;

    /*
    uv_os_fd_t handle;
    int status = uv_fileno((uv_handle_t*)m_server_handle, &handle);
    if (status < 0) {
        std::cout << uv_strerror(status) << std::endl;
    }
    // Enable SO_REUSEPORT on it, before we have a chance to listen on it.
    int enable = 1;
    setsockopt(handle, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable));
    */

    uv_ip4_addr(ip_addr_str.c_str(), port, &m_unix_addr);
    return uv_tcp_bind(m_server_handle, reinterpret_cast<const struct sockaddr*>(&m_unix_addr), 0);
}

int TcpServer::Impl::listen(NewConnectionCallback new_connection_callback,
                            DataReceivedCallback data_receive_callback,
                            int backlog_size) {
    m_new_connection_callback = new_connection_callback;
    m_data_receive_callback = data_receive_callback;
    int status = uv_listen(reinterpret_cast<uv_stream_t*>(m_server_handle), backlog_size, on_new_connection);

    // TODO: instead of returning status code need to call TcpServer::on_new_connection with appropriate status set
    if (status < 0) {
        std::cout << uv_strerror(status) << std::endl;
    }
    return status;
}

void TcpServer::Impl::shutdown() {
    for (auto& client : m_client_connections) {
        client->shutdown(); // TODO: shutdown with schedule_removal?????
    }

    m_client_connections.clear();

    // shutdown only works on connected sockets but m_server_handle does not connects to anyone
    /*
    auto shutdown_req = new uv_shutdown_t;
    int status = uv_shutdown(shutdown_req, reinterpret_cast<uv_stream_t*>(m_server_handle), TcpServer::on_shutdown);
    if (status < 0) {
        m_loop->log(Logger::Severity::DEBUG, "TcpServer::shutdown failer to start shutdown, reason: ", uv_strerror(status));
    }
    */

   if (m_server_handle && !uv_is_closing(reinterpret_cast<uv_handle_t*>(m_server_handle))) {
        uv_close(reinterpret_cast<uv_handle_t*>(m_server_handle), on_close);
   }
}

void TcpServer::Impl::close() {
    // TODO: clients close??????

    uv_close(reinterpret_cast<uv_handle_t*>(m_server_handle), on_close);
}

void TcpServer::Impl::remove_client_connection(TcpConnectedClient* client) {
    m_client_connections.erase(client);
    client->schedule_removal();
}

std::size_t TcpServer::Impl::connected_clients_count() const {
    return m_client_connections.size();
}

////////////////////////////////////////////// static //////////////////////////////////////////////
void TcpServer::Impl::on_new_connection(uv_stream_t* server, int status) {
    assert(server && "server should be not null");

    auto& this_ = *reinterpret_cast<TcpServer::Impl*>(server->data);

    IO_LOG(this_.m_loop, TRACE, "_");

    if (status < 0) {
        // TODO: need to find better error reporting solution in case when TOO_MANY_OPEN_FILES_ERROR
        //       because it spams log. For example, could record last error and log only when last error != current one
        IO_LOG(this_.m_loop, ERROR, "error:", uv_strerror(status));
        // TODO: error handling
        return;
    }

    auto tcp_client = new TcpConnectedClient(*this_.m_loop, *this_.m_parent);

    if (uv_accept(server, reinterpret_cast<uv_stream_t*>(tcp_client->tcp_client_stream())) == 0) {
        //sockaddr_storage info;
        struct sockaddr info;
        int info_len = sizeof info;
        int status = uv_tcp_getpeername(reinterpret_cast<uv_tcp_t*>(tcp_client->tcp_client_stream()),
                                        &info,
                                        &info_len);
        if (status == 0) {
            auto addr_info = reinterpret_cast<sockaddr_in*>(&info);
            tcp_client->set_port(network_to_host(addr_info->sin_port));
            tcp_client->set_ipv4_addr(network_to_host(addr_info->sin_addr.s_addr));

            bool allow_connection = true;
            if (this_.m_new_connection_callback) {
                allow_connection = this_.m_new_connection_callback(*this_.m_parent, *tcp_client);
            }

            if (allow_connection) {
                this_.m_client_connections.insert(tcp_client);

                tcp_client->start_read(this_.m_data_receive_callback);
            } else {
                // TODO: probably closing connection from the server side is not the best idea
                // We can send message to client that server is not ready and disconnect from client side
                // uv_close(reinterpret_cast<uv_handle_t*>(tcp_client), nullptr/*on_close*/);
                // close;
                tcp_client->schedule_removal();
            }

        } else {
            // TODO: call close???
            IO_LOG(this_.m_loop, ERROR, "uv_tcp_getpeername failed. Reason:", uv_strerror(status));
        }
    } else {
        //uv_close(reinterpret_cast<uv_handle_t*>(tcp_client), nullptr/*on_close*/);
        // TODO: schedule TcpConnectedClient removal here
    }
}

void TcpServer::Impl::on_close(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<TcpServer::Impl*>(handle->data);
    this_.m_server_handle = nullptr;
    delete reinterpret_cast<uv_tcp_t*>(handle);
}

// TODO: candidate for removal
void TcpServer::Impl::on_shutdown(uv_shutdown_t* req, int status) {
    uv_close(reinterpret_cast<uv_handle_t*>(req->handle), on_close);
    delete req;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TcpServer::TcpServer(EventLoop& loop) :
    /* Disposable(loop),*/
    m_impl(new Impl(loop, *this)) {
}

TcpServer::~TcpServer() {
}

int TcpServer::bind(const std::string& ip_addr_str, std::uint16_t port) {
    return m_impl->bind(ip_addr_str, port);
}

int TcpServer::listen(NewConnectionCallback new_connection_callback,
                      DataReceivedCallback data_receive_callback,
                      int backlog_size) {
    return m_impl->listen(new_connection_callback, data_receive_callback, backlog_size);
}

void TcpServer::shutdown() {
    return m_impl->shutdown();
}

void TcpServer::close() {
    return m_impl->close();
}

std::size_t TcpServer::connected_clients_count() const {
    return m_impl->connected_clients_count();
}

void TcpServer::remove_client_connection(TcpConnectedClient* client) {
    return m_impl->remove_client_connection(client);
}

} // namespace io

#include "TcpServer.h"

#include "ByteSwap.h"
#include "detail/Common.h"

#include <assert.h>

namespace io {

const size_t TcpServer::READ_BUFFER_SIZE;

class TcpServer::Impl {
public:
    Impl(EventLoop& loop, TcpServer& parent);
    ~Impl();

    Error listen(const Endpoint& endpoint,
                 NewConnectionCallback new_connection_callback,
                 DataReceivedCallback data_receive_callback,
                 CloseConnectionCallback close_connection_callback,
                 int backlog_size);

    void shutdown(ShutdownServerCallback shutdown_callback);
    void close(CloseServerCallback close_callback);

    std::size_t connected_clients_count() const;

    void remove_client_connection(TcpConnectedClient* client);

    bool schedule_removal();

protected:
    void close_impl();

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

    NewConnectionCallback m_new_connection_callback = nullptr;
    DataReceivedCallback m_data_receive_callback = nullptr;
    CloseConnectionCallback m_close_connection_callback = nullptr;

    CloseServerCallback m_end_server_callback = nullptr;

    // TODO: unordered set???
    std::set<TcpConnectedClient*> m_client_connections;

    // Made as unique_ptr because boost::pool has no move constructor defined
    //std::unique_ptr<boost::pool<>> m_pool;
};

TcpServer::Impl::Impl(EventLoop& loop, TcpServer& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop()))/*,
    m_pool(new boost::pool<>(TcpServer::READ_BUFFER_SIZE))*/ {
}

TcpServer::Impl::~Impl() {
}

Error TcpServer::Impl::listen(const Endpoint& endpoint,
                              NewConnectionCallback new_connection_callback,
                              DataReceivedCallback data_receive_callback,
                              CloseConnectionCallback close_connection_callback,
                              int backlog_size) {
    if (endpoint.type() == Endpoint::UNDEFINED) {
        return Error(StatusCode::INVALID_ARGUMENT);
    }

    m_server_handle = new uv_tcp_t;
    const auto init_status = uv_tcp_init_ex(m_uv_loop, m_server_handle, AF_INET); // TODO: IPV6 support
    m_server_handle->data = this;

    if (init_status < 0) {
        IO_LOG(m_loop, ERROR, m_parent, uv_strerror(init_status));
        return init_status;
    }

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

    const int bind_status = uv_tcp_bind(m_server_handle, reinterpret_cast<const struct sockaddr*>(endpoint.raw_endpoint()), 0);
    if (bind_status < 0) {
        IO_LOG(m_loop, ERROR, m_parent, "Bind failed:", uv_strerror(bind_status));
        return bind_status;
    }

    m_new_connection_callback = new_connection_callback;
    m_data_receive_callback = data_receive_callback;
    m_close_connection_callback = close_connection_callback;
    const int listen_status = uv_listen(reinterpret_cast<uv_stream_t*>(m_server_handle), backlog_size, on_new_connection);
    if (listen_status < 0) {
        IO_LOG(m_loop, ERROR, m_parent, "Listen failed:", uv_strerror(listen_status));
    }

    return listen_status;
}

void TcpServer::Impl::shutdown(ShutdownServerCallback shutdown_callback) {
    m_end_server_callback = shutdown_callback;

    for (auto& client : m_client_connections) {
        client->shutdown();
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

    close_impl();
}

void TcpServer::Impl::close(CloseServerCallback close_callback) {
    m_end_server_callback = close_callback;

    for (auto& client : m_client_connections) {
        client->close();
    }

    close_impl();
}

void TcpServer::Impl::close_impl() {
    if (m_server_handle && !uv_is_closing(reinterpret_cast<uv_handle_t*>(m_server_handle))) {
        uv_close(reinterpret_cast<uv_handle_t*>(m_server_handle), on_close);
    }
}

void TcpServer::Impl::remove_client_connection(TcpConnectedClient* client) {
    m_client_connections.erase(client);
}

std::size_t TcpServer::Impl::connected_clients_count() const {
    return m_client_connections.size();
}

bool TcpServer::Impl::schedule_removal() {
    const auto removal_scheduled = m_parent->is_removal_scheduled();

    if (!removal_scheduled) {
        m_parent->set_removal_scheduled();

        auto end_server_callback_copy = m_end_server_callback;

        this->close([this, end_server_callback_copy] (TcpServer& server, const Error& error) {
            if (end_server_callback_copy) {
                end_server_callback_copy(server, error);
            }
        });
    }

    return removal_scheduled || m_server_handle == nullptr;
}

////////////////////////////////////////////// static //////////////////////////////////////////////
void TcpServer::Impl::on_new_connection(uv_stream_t* server, int status) {
    assert(server && "server should be not null");

    auto& this_ = *reinterpret_cast<TcpServer::Impl*>(server->data);

    IO_LOG(this_.m_loop, TRACE, "_");

    if (status < 0) {
        // TODO: error handling
        // TODO: need to find better error reporting solution in case when TOO_MANY_OPEN_FILES_ERROR
        //       because it spams log. For example, could record last error and log only when last error != current one
        IO_LOG(this_.m_loop, ERROR, "error:", uv_strerror(status));
        return;
    }

    auto on_client_close_callback = [&this_](TcpConnectedClient& client, const Error& error) {
        if (this_.m_close_connection_callback) {
            this_.m_close_connection_callback(client, error);
        }
    };

    auto tcp_client = new TcpConnectedClient(*this_.m_loop, *this_.m_parent, on_client_close_callback);

    auto accept_status = uv_accept(server, reinterpret_cast<uv_stream_t*>(tcp_client->tcp_client_stream()));
    if (accept_status == 0) {
        //sockaddr_storage info;
        struct sockaddr info;
        int info_len = sizeof info;
        int getpeername_status = uv_tcp_getpeername(reinterpret_cast<uv_tcp_t*>(tcp_client->tcp_client_stream()),
                                                    &info,
                                                    &info_len);
        if (getpeername_status == 0) {
            auto addr_info = reinterpret_cast<sockaddr_in*>(&info);
            tcp_client->set_port(network_to_host(addr_info->sin_port));
            tcp_client->set_ipv4_addr(network_to_host(addr_info->sin_addr.s_addr));

            this_.m_client_connections.insert(tcp_client);
            this_.m_new_connection_callback(*tcp_client, Error(0));
            if (tcp_client->is_open()) {
                tcp_client->start_read(this_.m_data_receive_callback);
            }
        } else {
            // TODO: call close???
            IO_LOG(this_.m_loop, ERROR, "uv_tcp_getpeername failed. Reason:", uv_strerror(getpeername_status));
            this_.m_new_connection_callback(*tcp_client, Error(getpeername_status));
        }
    } else {
        this_.m_new_connection_callback(*tcp_client, Error(accept_status));
        //uv_close(reinterpret_cast<uv_handle_t*>(tcp_client), nullptr/*on_close*/);
        // TODO: schedule TcpConnectedClient removal here
    }
}

void TcpServer::Impl::on_close(uv_handle_t* handle) {
    if (handle->data) {
        auto& this_ = *reinterpret_cast<TcpServer::Impl*>(handle->data);
        if (this_.m_end_server_callback) {
            this_.m_end_server_callback(*this_.m_parent, Error(0));
        }

        if (this_.m_parent->is_removal_scheduled()) {
            this_.m_parent->schedule_removal();
        }

        this_.m_server_handle = nullptr;
    }

    delete reinterpret_cast<uv_tcp_t*>(handle);
}

// TODO: candidate for removal
void TcpServer::Impl::on_shutdown(uv_shutdown_t* req, int status) {
    uv_close(reinterpret_cast<uv_handle_t*>(req->handle), on_close);
    delete req;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TcpServer::TcpServer(EventLoop& loop) :
    Removable(loop),
    m_impl(new Impl(loop, *this)) {
}

TcpServer::~TcpServer() {
}

Error TcpServer::listen(const Endpoint& endpoint,
                        NewConnectionCallback new_connection_callback,
                        DataReceivedCallback data_receive_callback,
                        CloseConnectionCallback close_connection_callback,
                        int backlog_size) {
    return m_impl->listen(endpoint, new_connection_callback, data_receive_callback, close_connection_callback, backlog_size);
}

void TcpServer::shutdown(ShutdownServerCallback shutdown_callback) {
    return m_impl->shutdown(shutdown_callback);
}

void TcpServer::close(CloseServerCallback close_callback) {
    return m_impl->close(close_callback);
}

std::size_t TcpServer::connected_clients_count() const {
    return m_impl->connected_clients_count();
}

void TcpServer::remove_client_connection(TcpConnectedClient* client) {
    return m_impl->remove_client_connection(client);
}

void TcpServer::schedule_removal() {
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

} // namespace io

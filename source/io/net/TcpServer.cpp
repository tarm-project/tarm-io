/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/


#include "detail/Common.h"
#include "detail/LogMacros.h"
#include "net/TcpServer.h"

#include "ByteSwap.h"

#include <unordered_set>

#include <assert.h>

namespace tarm {
namespace io {
namespace net {

const size_t TcpServer::READ_BUFFER_SIZE;

class TcpServer::Impl {
public:
    Impl(EventLoop& loop, TcpServer& parent);
    ~Impl();

    Error listen(const Endpoint& endpoint,
                 const NewConnectionCallback& new_connection_callback,
                 const DataReceivedCallback& data_receive_callback,
                 const CloseConnectionCallback& close_connection_callback,
                 int backlog_size);

    void shutdown(const ShutdownServerCallback& shutdown_callback);
    void close(const CloseServerCallback& close_callback);

    std::size_t connected_clients_count() const;

    void remove_client_connection(TcpConnectedClient* client);

    const Endpoint& endpoint() const;

    bool schedule_removal();

protected:
    void close_impl();

    bool is_open() const;

    // statics
    static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

    static void on_new_connection(uv_stream_t* server, int status);

    static void on_close(uv_handle_t* handle);

private:
    TcpServer* m_parent;
    EventLoop* m_loop;
    uv_loop_t* m_uv_loop;

    uv_tcp_t* m_server_handle = nullptr;

    NewConnectionCallback m_new_connection_callback = nullptr;
    DataReceivedCallback m_data_receive_callback = nullptr;
    CloseConnectionCallback m_close_connection_callback = nullptr;

    CloseServerCallback m_end_server_callback = nullptr;

    std::unordered_set<TcpConnectedClient*> m_client_connections;

    Endpoint m_endpoint;

    bool m_is_open = false;
};

TcpServer::Impl::Impl(EventLoop& loop, TcpServer& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop()))/*,
    m_pool(new boost::pool<>(TcpServer::READ_BUFFER_SIZE))*/ {
}

TcpServer::Impl::~Impl() {
    LOG_TRACE(m_loop, m_parent);
}

Error TcpServer::Impl::listen(const Endpoint& endpoint,
                              const NewConnectionCallback& new_connection_callback,
                              const DataReceivedCallback& data_receive_callback,
                              const CloseConnectionCallback& close_connection_callback,
                              int backlog_size) {
    if (endpoint.type() == Endpoint::UNDEFINED) {
        return Error(StatusCode::INVALID_ARGUMENT);
    }

    if (m_server_handle) {
        return StatusCode::CONNECTION_ALREADY_IN_PROGRESS;
    }

    m_endpoint = endpoint;

    m_server_handle = new uv_tcp_t;
    const Error init_error = uv_tcp_init_ex(m_uv_loop,
                                            m_server_handle,
                                            endpoint.type() == Endpoint::IP_V4 ? AF_INET : AF_INET6);
    m_server_handle->data = this;

    if (init_error) {
        LOG_ERROR(m_loop, m_parent, init_error.string());
        return init_error;
    }

    const int bind_status = uv_tcp_bind(m_server_handle, reinterpret_cast<const struct sockaddr*>(m_endpoint.raw_endpoint()), 0);
    if (bind_status < 0) {
        LOG_ERROR(m_loop, m_parent, "Bind failed:", uv_strerror(bind_status));
        return bind_status;
    }

    m_new_connection_callback = new_connection_callback;
    m_data_receive_callback = data_receive_callback;
    m_close_connection_callback = close_connection_callback;
    const int listen_status = uv_listen(reinterpret_cast<uv_stream_t*>(m_server_handle), backlog_size, on_new_connection);
    if (listen_status < 0) {
        LOG_ERROR(m_loop, m_parent, "Listen failed:", uv_strerror(listen_status));
    }

    m_is_open = true;

    return listen_status;
}

const Endpoint& TcpServer::Impl::endpoint() const {
    return m_endpoint;
}

void TcpServer::Impl::shutdown(const ShutdownServerCallback& shutdown_callback) {
    LOG_TRACE(m_loop, m_parent);

    m_end_server_callback = shutdown_callback;

    for (auto& client : m_client_connections) {
        client->shutdown();
    }

    close_impl();
}

void TcpServer::Impl::close(const CloseServerCallback& close_callback) {
    LOG_TRACE(m_loop, m_parent, "");

    m_end_server_callback = close_callback;

    for (auto& client : m_client_connections) {
        client->close();
    }

    close_impl();
}

void TcpServer::Impl::close_impl() {
    LOG_TRACE(m_loop, m_parent, "");

    if (m_server_handle && !uv_is_closing(reinterpret_cast<uv_handle_t*>(m_server_handle))) {
        uv_close(reinterpret_cast<uv_handle_t*>(m_server_handle), on_close);
    } else {
        if (m_end_server_callback) {
            m_loop->schedule_callback([=](EventLoop&) {
                m_end_server_callback(*m_parent, Error(StatusCode::NOT_CONNECTED));
            });
        }
    }

    m_is_open = false;
}

bool TcpServer::Impl::is_open() const {
    return m_is_open;
}

void TcpServer::Impl::remove_client_connection(TcpConnectedClient* client) {
    m_client_connections.erase(client);
}

std::size_t TcpServer::Impl::connected_clients_count() const {
    return m_client_connections.size();
}

bool TcpServer::Impl::schedule_removal() {
    LOG_TRACE(m_loop, m_parent);

    const auto removal_scheduled = m_parent->is_removal_scheduled();

    if (!removal_scheduled && m_server_handle) {
        m_parent->set_removal_scheduled();
        this->close(nullptr);
        return false;
    }

    return true;
}

////////////////////////////////////////////// static //////////////////////////////////////////////
void TcpServer::Impl::on_new_connection(uv_stream_t* server, int status) {
    assert(server && "server should be not null");
    assert(server->data && "server should have user data set");

    auto& this_ = *reinterpret_cast<TcpServer::Impl*>(server->data);

    LOG_TRACE(this_.m_loop, this_.m_parent);

    auto on_client_close_callback = [&this_](TcpConnectedClient& client, const Error& error) {
        if (this_.m_close_connection_callback) {
            this_.m_close_connection_callback(client, error);
        }
    };

    std::unique_ptr<TcpConnectedClient, std::function<void(TcpConnectedClient*)>> tcp_client(
        new TcpConnectedClient(*this_.m_loop, *this_.m_parent, on_client_close_callback),
        [](TcpConnectedClient* c) {
            c->schedule_removal();
        }
    );
    const auto init_error = tcp_client->init_stream();
    if (init_error) {
        LOG_ERROR(this_.m_loop, this_.m_parent, "init_error");
        if (this_.m_new_connection_callback) {
            this_.m_new_connection_callback(*tcp_client, init_error);
        }
        return;
    }

    const Error error(status);
    if (error) {
        LOG_ERROR(this_.m_loop, this_.m_parent, error);
        if (this_.m_new_connection_callback) {
            this_.m_new_connection_callback(*tcp_client, error);
        }
        return;
    }

    auto accept_status = uv_accept(server, reinterpret_cast<uv_stream_t*>(tcp_client->tcp_client_stream()));
    if (accept_status == 0) {
        //sockaddr_storage info;
        struct sockaddr_storage info;
        int info_len = sizeof info;
        Error getpeername_error = uv_tcp_getpeername(reinterpret_cast<uv_tcp_t*>(tcp_client->tcp_client_stream()),
                                                     reinterpret_cast<struct sockaddr*>(&info),
                                                     &info_len);
        if (!getpeername_error) {
            tcp_client->set_endpoint(Endpoint(reinterpret_cast<const Endpoint::sockaddr_placeholder*>(&info)));

            this_.m_client_connections.insert(tcp_client.get());

            if (this_.m_new_connection_callback) {
                this_.m_new_connection_callback(*tcp_client, Error(0));
            }

            if (tcp_client->is_open()) {
                tcp_client->start_read(this_.m_data_receive_callback);
            }

            tcp_client.release();
        } else {
            LOG_ERROR(this_.m_loop, "uv_tcp_getpeername failed. Reason:", getpeername_error.string());
            if (this_.m_new_connection_callback) {
                if (getpeername_error.code() == StatusCode::INVALID_ARGUMENT) {
                    // Apple has incompatibility here
                    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/getpeername.2.html
                    // The same is true for Mac OS X
                    // It is safe for other Unix-es because they do not return EINVAL in this case
                    this_.m_new_connection_callback(*tcp_client, StatusCode::OK);
                    this_.m_close_connection_callback(*tcp_client, StatusCode::CONNECTION_RESET_BY_PEER);
                } else {
                    this_.m_new_connection_callback(*tcp_client, getpeername_error);
                }
            }

            tcp_client.reset();
        }
    } else {
        this_.m_new_connection_callback(*tcp_client, Error(accept_status));
        return;
    }
}

void TcpServer::Impl::on_close(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<TcpServer::Impl*>(handle->data);
    LOG_TRACE(this_.m_loop, this_.m_parent);

    if (this_.connected_clients_count() != 0) {
        // Waiting for all clients to be closed
        this_.m_loop->schedule_callback([handle](EventLoop&) {
                on_close(handle);
            }
        );
        return;
    }

    if (this_.m_end_server_callback) {
        auto end_callback_copy = std::move(this_.m_end_server_callback);
        this_.m_loop->schedule_callback([end_callback_copy, handle, &this_](EventLoop&) {
            end_callback_copy(*this_.m_parent, Error(0));

            //if (this_.m_parent->is_removal_scheduled()) {
            //    this_.m_parent->schedule_removal();
            //}

            delete reinterpret_cast<uv_tcp_t*>(handle);
        });
    } else {
        if (this_.m_parent->is_removal_scheduled()) {
            this_.m_parent->schedule_removal();
        }
        delete reinterpret_cast<uv_tcp_t*>(handle);
    }

    this_.m_server_handle = nullptr;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TcpServer::TcpServer(EventLoop& loop) :
    Removable(loop),
    m_impl(new Impl(loop, *this)) {
}

TcpServer::~TcpServer() {
}

Error TcpServer::listen(const Endpoint& endpoint,
                        const NewConnectionCallback& new_connection_callback,
                        const DataReceivedCallback& data_receive_callback,
                        const CloseConnectionCallback& close_connection_callback,
                        int backlog_size) {
    return m_impl->listen(endpoint, new_connection_callback, data_receive_callback, close_connection_callback, backlog_size);
}

void TcpServer::shutdown(const ShutdownServerCallback& shutdown_callback) {
    return m_impl->shutdown(shutdown_callback);
}

void TcpServer::close(const CloseServerCallback& close_callback) {
    return m_impl->close(close_callback);
}

std::size_t TcpServer::connected_clients_count() const {
    return m_impl->connected_clients_count();
}

void TcpServer::remove_client_connection(TcpConnectedClient* client) {
    return m_impl->remove_client_connection(client);
}

const Endpoint& TcpServer::endpoint() const {
    return m_impl->endpoint();
}

void TcpServer::schedule_removal() {
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

} // namespace net
} // namespace io
} // namespace tarm

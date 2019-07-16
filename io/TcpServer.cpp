#include "TcpServer.h"

#include <assert.h>

// TODO: move
#include <iostream>
#include <string>
#include <iomanip>

namespace io {

TcpServer::TcpServer(EventLoop& loop) :
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())),
    m_pool(std::make_unique<boost::pool<>>(TcpServer::READ_BUFFER_SIZE)) {
}

TcpServer::~TcpServer() {
    //shutdown();
}

int TcpServer::bind(const std::string& ip_addr_str, std::uint16_t port) {
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

int TcpServer::listen(NewConnectionCallback new_connection_callback,
                      DataReceivedCallback data_receive_callback,
                      int backlog_size) {
    m_new_connection_callback = new_connection_callback;
    m_data_receive_callback = data_receive_callback;
    int status = uv_listen(reinterpret_cast<uv_stream_t*>(m_server_handle), backlog_size, TcpServer::on_new_connection);

    // TODO: instead of returning status code need to call TcpServer::on_new_connection with appropriate status set
    if (status < 0) {
        std::cout << uv_strerror(status) << std::endl;
    }
    return status;
}

void TcpServer::shutdown() {
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
        uv_close(reinterpret_cast<uv_handle_t*>(m_server_handle), TcpServer::on_close);
   }
}

void TcpServer::close() {
    // TODO: clients close??????

    uv_close(reinterpret_cast<uv_handle_t*>(m_server_handle), TcpServer::on_close);
}

void TcpServer::remove_client_connection(TcpClient* client) {
    m_client_connections.erase(client);
    client->schedule_removal();
}

std::size_t TcpServer::connected_clients_count() const {
    return m_client_connections.size();
}

// ////////////////////////////////////// static //////////////////////////////////////

void TcpServer::alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    // TODO: probably use suggested_size via handling hash table of different pools by block size

    auto& client = *reinterpret_cast<TcpClient*>(handle->data);
    auto& this_ = client.server();
    buf->base = reinterpret_cast<char*>(this_.m_pool->malloc());
    buf->len = this_.READ_BUFFER_SIZE;
}

void TcpServer::on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
    if (client->data == nullptr) {
        // TODO: error handling here
    }

    // TODO: capture buf by unique_ptr
    // buf

    auto& tcp_client = *reinterpret_cast<TcpClient*>(client->data);
    auto& this_ = tcp_client.server();

    if (nread > 0) {
        if (this_.m_data_receive_callback) {
            this_.m_data_receive_callback(this_, tcp_client, buf->base, nread);
        }
        return;
    }

    if (nread < 0) {
        if (nread == UV_EOF) {
            this_.m_loop->log(Logger::Severity::TRACE, "TcpServer::on_read connection end ",
                              io::ip4_addr_to_string(tcp_client.ipv4_addr()), ":", tcp_client.port());
            //tcp_client.schedule_removal();
            this_.remove_client_connection(&tcp_client);
        }

        // TODO: this code was replaced by remove_client_connection. Remove it later
        /*
        auto it = this_.m_client_connections.find(&tcp_client);
        if (it != this_.m_client_connections.end()) {
            this_.m_client_connections.erase(it); // TODO: who will delete client's memory?????
        } else {
            // error unknown connection
        }
         */
        //uv_close((uv_handle_t*) client, nullptr /*on_close*/);
    }

    this_.m_pool->free(buf->base);
}

void TcpServer::on_new_connection(uv_stream_t* server, int status) {
    assert(server && "server should be not null");

    auto& this_ = *reinterpret_cast<TcpServer*>(server->data);

    this_.m_loop->log(Logger::Severity::TRACE, "TcpServer::on_new_connection");

    if (status < 0) {
        // TODO: need to find better error reporting solution in case when TOO_MANY_OPEN_FILES_ERROR
        //       because it spams log. For example, could record last error and log only when last error != current one
        this_.m_loop->log(Logger::Severity::ERROR, "TcpServer::on_new_connection error: ", uv_strerror(status));
        // TODO: error handling
        return;
    }

    auto tcp_client = new TcpClient(*this_.m_loop, this_);

    if (uv_accept(server, reinterpret_cast<uv_stream_t*>(tcp_client->tcp_client_stream())) == 0) {
        //sockaddr_storage info;
        struct sockaddr info;
        int info_len = sizeof info;
        int status = uv_tcp_getpeername(tcp_client->tcp_client_stream(),
                                        &info,
                                        &info_len);
        if (status == 0) {
            auto addr_info = reinterpret_cast<sockaddr_in*>(&info);
            tcp_client->set_port(ntohs(addr_info->sin_port));
            tcp_client->set_ipv4_addr(ntohl(addr_info->sin_addr.s_addr));

            bool allow_connection = true;
            if (this_.m_new_connection_callback) {
                allow_connection = this_.m_new_connection_callback(this_, *tcp_client);
            }

            if (allow_connection) {
                this_.m_client_connections.insert(tcp_client);

                uv_read_start(reinterpret_cast<uv_stream_t*>(tcp_client->tcp_client_stream()),
                              TcpServer::alloc_buffer,
                              TcpServer::on_read);
            } else {
                // TODO: probably closing connection from the server side is not the best idea
                // We can send message to client that server is not ready and disconnect from client side
                // uv_close(reinterpret_cast<uv_handle_t*>(tcp_client), nullptr/*on_close*/);
                // close;
                tcp_client->schedule_removal();
            }

        } else {
            // TODO: call close???
            this_.m_loop->log(Logger::Severity::ERROR, "uv_tcp_getpeername failed. Reason: ", uv_strerror(status));
        }
    } else {
        //uv_close(reinterpret_cast<uv_handle_t*>(tcp_client), nullptr/*on_close*/);
        // TODO: schedule TcpClient removal here
    }
}

void TcpServer::on_close(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<TcpServer*>(handle->data);
    this_.m_server_handle = nullptr;
    delete reinterpret_cast<uv_tcp_t*>(handle);
}

// TODO: candidate for removal
void TcpServer::on_shutdown(uv_shutdown_t* req, int status) {
    uv_close(reinterpret_cast<uv_handle_t*>(req->handle), TcpServer::on_close);
    delete req;
}

} // namespace io

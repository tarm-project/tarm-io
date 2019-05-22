#include "TcpServer.h"

#include <assert.h>

// TODO: move
#include <iostream>
#include <string>
#include <iomanip>

namespace io {

TcpServer::TcpServer(EventLoop& loop) :
    m_loop(&loop),
    m_pool(std::make_unique<boost::pool<>>(TcpServer::READ_BUFFER_SIZE)) {
    uv_tcp_init(&loop, this);
}

TcpServer::~TcpServer() {
    //shutdown();
}

int TcpServer::bind(const std::string& ip_addr_str, std::uint16_t port) {
    uv_ip4_addr(ip_addr_str.c_str(), port, &m_unix_addr);
    return uv_tcp_bind(this, reinterpret_cast<const struct sockaddr*>(&m_unix_addr), 0);
}

int TcpServer::listen(DataReceivedCallback data_receive_callback,
                      NewConnectionCallback new_connection_callback,
                      int backlog_size) {
    m_data_receive_callback = data_receive_callback;
    m_new_connection_callback = new_connection_callback;
    return uv_listen((uv_stream_t*) this, backlog_size, TcpServer::on_new_connection);
}

void TcpServer::shutdown() {
    for (auto& v : m_client_connections) {
        // TODO: close from client side??? See comment below
        //uv_close(reinterpret_cast<uv_handle_t*>(key_value.first), nullptr /*on_close*/);

        v->shutdown();
    }


    //uv_shutdown

    // auto shutdown_req = new uv_shutdown_t;
    //uv_shutdown(shutdown_req, reinterpret_cast<uv_stream_t*>(this), TcpServer::on_server_shutdown);

    uv_close(reinterpret_cast<uv_handle_t*>(this), nullptr /*on_close*/);
}

// ////////////////////////////////////// static //////////////////////////////////////
//static void close_cb(uv_handle_t* handle) {
//
//  free(conn);
//}

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
            std::cout << "Connection end!" << std::endl;
        }

        auto it = this_.m_client_connections.find(&tcp_client);
        if (it != this_.m_client_connections.end()) {
            this_.m_client_connections.erase(it);
        } else {
            // error unknown connection
        }

        uv_close((uv_handle_t*) client, nullptr /*on_close*/);
    }

    this_.m_pool->free(buf->base);

    //delete[] buf->base;
}

void TcpServer::on_new_connection(uv_stream_t* server, int status) {
    assert(server && "server should be not null");
    std::cout << "on_new_connection" << std::endl;
    if (status < 0) {
        // TODO: error handling!
        //fprintf(stderr, "New connection error %s\n", uv_strerror(status));
        // error!
        return;
    }

    auto& this_ = reinterpret_cast<TcpServer&>(*server);
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
            }

        } else {
            // close???
            std::cerr << "[Server] uv_tcp_getpeername failed. Reason: " << uv_strerror(status) << std::endl;
        }
    } else {
        //uv_close(reinterpret_cast<uv_handle_t*>(tcp_client), nullptr/*on_close*/);
        // TODO: schedule TcpClient removal here
    }
}

} // namespace io
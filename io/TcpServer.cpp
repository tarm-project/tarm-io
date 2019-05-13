#include "TcpServer.h"

#include <assert.h>

// TODO: move
#include <iostream>
#include <string>
#include <iomanip>

namespace io {

TcpServer::TcpServer(EventLoop& loop) :
    m_loop(loop),
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
    for (auto& key_value : m_client_connections) {
        // TODO: close from client side??? See comment below
        //uv_close(reinterpret_cast<uv_handle_t*>(key_value.first), nullptr /*on_close*/);

        auto shutdown_req = new uv_shutdown_t;
        uv_shutdown(shutdown_req, reinterpret_cast<uv_stream_t*>(key_value.first), TcpServer::on_client_shutdown);
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

    auto& this_ = *reinterpret_cast<TcpServer*>(handle->data);
    buf->base = reinterpret_cast<char*>(this_.m_pool->malloc());
    buf->len = this_.READ_BUFFER_SIZE;
}

void TcpServer::on_client_close_cb(uv_handle_t* handle) {
    uv_print_all_handles(handle->loop, stdout);
}


void TcpServer::on_client_shutdown(uv_shutdown_t* req, int status) {
    std::cout << "on_client_shutdown" << std::endl;
    uv_close(reinterpret_cast<uv_handle_t*>(req->handle), TcpServer::on_client_close_cb);

    delete req;
}

void TcpServer::on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
    if (client->data == nullptr) {
        // TODO: error handling here
    }

    // TODO: capture buf by unique_ptr
    // buf

    auto& this_ = *reinterpret_cast<TcpServer*>(client->data);

    if (nread > 0) {
        /*
        write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
        req->buf = uv_buf_init(buf->base, nread);
        uv_write((uv_write_t*) req, client, &req->buf, 1, echo_write);
        */

        /*
        std::string message(buf->base, buf->base + nread);
        if (message.find("close") != std::string::npos) {
            //uv_close(reinterpret_cast<uv_handle_t*>(client), nullptr);
            exit(1);
            //uv_stop(client->loop);
        }

        //std::cout.write(buf->base, nread);
        std::cout << message;
        */

        if (this_.m_data_receive_callback) {
            this_.m_data_receive_callback(this_, *reinterpret_cast<TcpClient*>(client), buf->base, nread);
        }
        return;
    }

    if (nread < 0) {
        if (nread == UV_EOF) {
            std::cout << "Connection end!" << std::endl;
        }

        // if (nread != UV_EOF)
        //     fprintf(stderr, "Read error %s\n", uv_err_name(nread));

        auto it = this_.m_client_connections.find(reinterpret_cast<uv_tcp_t*>(client));
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

    /*

    */

    auto& this_ = reinterpret_cast<TcpServer&>(*server);
    auto tcp_client_uptr = std::make_unique<TcpClient>();
    auto tcp_client = tcp_client_uptr.get();
    tcp_client->data = &this_;

    uv_tcp_init(&this_.m_loop, tcp_client);

    if (uv_accept(server, reinterpret_cast<uv_stream_t*>(tcp_client)) == 0) {
        //sockaddr_storage info;
        struct sockaddr info;
        int info_len = sizeof info;
        int status = uv_tcp_getpeername(reinterpret_cast<const uv_tcp_t*>(tcp_client),
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
                this_.m_client_connections.insert(std::make_pair(tcp_client_uptr.get(), std::move(tcp_client_uptr)));

                uv_read_start(reinterpret_cast<uv_stream_t*>(tcp_client),
                              TcpServer::alloc_buffer,
                              TcpServer::on_read);
            } else {
                // TODO: probably closing connection from the server side is not the best idea
                // We can send message to client that server is not ready and disconnect from client side
                uv_close(reinterpret_cast<uv_handle_t*>(tcp_client), nullptr/*on_close*/);
                // close;
            }

        } else {
            // close???
            std::cerr << "[Server] uv_tcp_getpeername failed. Reason: " << uv_strerror(status) << std::endl;
        }
    } else {
        uv_close(reinterpret_cast<uv_handle_t*>(tcp_client), nullptr/*on_close*/);
    }
}

} // namespace io
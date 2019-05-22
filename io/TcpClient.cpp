#include "TcpClient.h"

#include "TcpServer.h"

#include <assert.h>

// TODO: move
#include <iostream>

namespace io {

TcpClient::TcpClient(EventLoop& loop, TcpServer& server) :
    Disposable(loop),
    m_server(&server) {
    uv_tcp_init(&loop, &m_stream);
    m_stream.data = this;
}

TcpClient::~TcpClient() {
    std::cout << "TcpClient::~TcpClient" << std::endl;
    uv_close(reinterpret_cast<uv_handle_t*>(&m_stream), nullptr/*on_close*/);
}

std::uint32_t TcpClient::ipv4_addr() const {
    return m_ipv4_addr;
}

std::uint16_t TcpClient::port() const {
    return m_port;
}

void TcpClient::set_ipv4_addr(std::uint32_t value) {
    m_ipv4_addr = value;
}

void TcpClient::set_port(std::uint16_t value) {
    m_port = value;
}

void TcpClient::shutdown() {
    auto shutdown_req = new uv_shutdown_t;
    uv_shutdown(shutdown_req, reinterpret_cast<uv_stream_t*>(&m_stream), TcpClient::on_shutdown);
}

namespace {

struct WriteRequest : public uv_write_t {
    uv_buf_t buf;
};

} // namespace

void TcpClient::send_data(const char* buffer, std::size_t size, EndSendCallback callback) {
    m_end_send_callback = callback;

    auto req = new WriteRequest;
    req->data = this;
    // const_cast is workaround for lack of constness support in uv_buf_t
    req->buf = uv_buf_init(const_cast<char*>(buffer), size);

    uv_write(req, reinterpret_cast<uv_stream_t*>(&m_stream), &req->buf, 1, TcpClient::after_write);
    ++m_pending_write_requesets;
}

std::size_t TcpClient::pending_write_requesets() const {
    return m_pending_write_requesets;
}

const TcpServer& TcpClient::server() const {
    return *m_server;
}

TcpServer& TcpClient::server() {
    return *m_server;
}

uv_tcp_t* TcpClient::tcp_client_stream() {
    return &m_stream;
}

// ////////////////////////////////////// static //////////////////////////////////////
void TcpClient::after_write(uv_write_t* req, int status) {
    auto& this_ = *reinterpret_cast<TcpClient*>(req->data);

    assert(this_.m_pending_write_requesets >= 1);
    --this_.m_pending_write_requesets;

    auto request = reinterpret_cast<WriteRequest*>(req);
    delete request;

    if (status < 0) {
        std::cerr << "TcpClient::after_write: " << uv_strerror(status) << std::endl;
    }

    if (this_.m_end_send_callback) {
        this_.m_end_send_callback(this_);
    }
}

void TcpClient::on_close_cb(uv_handle_t* handle) {
    uv_print_all_handles(handle->loop, stdout); // TODO: remove
}


void TcpClient::on_shutdown(uv_shutdown_t* req, int status) {
    std::cout << "on_client_shutdown" << std::endl;
    uv_close(reinterpret_cast<uv_handle_t*>(req->handle), TcpClient::on_close_cb);

    delete req;
}

} // namespace io

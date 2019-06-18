#include "TcpClient.h"

#include "TcpServer.h"

#include <assert.h>

// TODO: move
#include <iostream>

namespace io {

TcpClient::TcpClient(EventLoop& loop) :
    Disposable(loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())) {
}

TcpClient::TcpClient(EventLoop& loop, TcpServer& server) :
    TcpClient(loop) {
    m_is_open = true;
    m_server = &server;
    init_stream();
}

TcpClient::~TcpClient() {
    m_loop->log(Logger::Severity::ERROR, "TcpClient::~TcpClient");

    if (m_connect_req) {
        delete m_connect_req; // TODO: delete right after connect???
    }

    if (m_tcp_stream) {
        delete m_tcp_stream;
    }

    //close();
}

void TcpClient::init_stream() {
    if (m_tcp_stream == nullptr) {
        m_tcp_stream = new uv_tcp_t;
        uv_tcp_init(m_uv_loop, m_tcp_stream);
        m_tcp_stream->data = this;
    }
}

void TcpClient::connect(const std::string& address, std::uint16_t port, ConnectCallback callback) {
    struct sockaddr_in addr;

    uv_ip4_addr(address.c_str(), port, &addr); // TODO: error handling

    if (m_connect_req == nullptr) {
        m_connect_req = new uv_connect_t;
        m_connect_req->data = this;
    }

    m_port = port;
    m_ipv4_addr = ntohl(addr.sin_addr.s_addr);

    m_loop->log(Logger::Severity::DEBUG, "TcpClient::connect ", io::ip4_addr_to_string(m_ipv4_addr));

    init_stream();
    m_connect_callback = callback;

    uv_tcp_connect(m_connect_req, m_tcp_stream, reinterpret_cast<const struct sockaddr*>(&addr), TcpClient::on_connect);
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
    shutdown_req->data = this;
    uv_shutdown(shutdown_req, reinterpret_cast<uv_stream_t*>(m_tcp_stream), TcpClient::on_shutdown);
}

namespace {

struct WriteRequest : public uv_write_t {
    uv_buf_t uv_buf;
    std::shared_ptr<const char> buf;
    TcpClient::EndSendCallback end_send_callback = nullptr;
};

} // namespace

void TcpClient::send_data(std::shared_ptr<const char> buffer, std::size_t size, EndSendCallback callback) {
    auto req = new WriteRequest;
    req->end_send_callback = callback;
    req->data = this;
    req->buf = buffer;
    // const_cast is a workaround for lack of constness support in uv_buf_t
    req->uv_buf = uv_buf_init(const_cast<char*>(buffer.get()), size);

    uv_write(req, reinterpret_cast<uv_stream_t*>(m_tcp_stream), &req->uv_buf, 1, TcpClient::after_write);
    ++m_pending_write_requesets;
}

void TcpClient::send_data(const std::string& message, EndSendCallback callback) {
    std::shared_ptr<char> ptr(new char[message.size()], [](const char* p) { delete[] p;});
    std::copy(message.c_str(), message.c_str() + message.size(), ptr.get());
    send_data(ptr, message.size(), callback);
}

void TcpClient::set_close_callback(CloseCallback callback) {
    m_close_callback = callback;
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
    return m_tcp_stream;
}

void TcpClient::schedule_removal() {
    m_loop->log(Logger::Severity::TRACE, "TcpClient::schedule_removal ", io::ip4_addr_to_string(m_ipv4_addr), ":", port());

    close();

    Disposable::schedule_removal();
}

void TcpClient::close() {
    if (!is_open()) {
        return;
    }

    m_loop->log(Logger::Severity::TRACE, "TcpClient::close ", io::ip4_addr_to_string(m_ipv4_addr), ":", port());

    m_is_open = false;

    if (m_close_callback) {
        m_close_callback(*this);
    }

    if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(m_tcp_stream))) {
        uv_close(reinterpret_cast<uv_handle_t*>(m_tcp_stream), TcpClient::on_close);
        m_tcp_stream = nullptr;
    }
}

bool TcpClient::is_open() const {
    return m_is_open;
}

// ////////////////////////////////////// static //////////////////////////////////////
void TcpClient::after_write(uv_write_t* req, int status) {
    auto& this_ = *reinterpret_cast<TcpClient*>(req->data);

    assert(this_.m_pending_write_requesets >= 1);
    --this_.m_pending_write_requesets;

    auto request = reinterpret_cast<WriteRequest*>(req);
    std::unique_ptr<WriteRequest> guard(request);

    if (status < 0) {
        std::cerr << "TcpClient::after_write: " << uv_strerror(status) << std::endl;
    }

    if (request->end_send_callback) {
        request->end_send_callback(this_);
    }
}

void TcpClient::on_shutdown(uv_shutdown_t* req, int status) {
    auto& this_ = *reinterpret_cast<TcpClient*>(req->data);

    this_.m_loop->log(Logger::Severity::TRACE, "TcpClient::on_shutdown ", io::ip4_addr_to_string(this_.m_ipv4_addr), ":", this_.port());

    this_.schedule_removal();

    //uv_close(reinterpret_cast<uv_handle_t*>(req->handle), TcpClient::on_close_cb);
    delete req;
}

void TcpClient::on_connect(uv_connect_t* req, int status) {
    auto& this_ = *reinterpret_cast<TcpClient*>(req->data);
    this_.m_is_open = true; // if not error!

    if (this_.m_connect_callback) {
        this_.m_connect_callback(this_);
    }
}

void TcpClient::on_close(uv_handle_t* handle) {
    if (handle->data) {
        auto& this_ = *reinterpret_cast<TcpClient*>(handle->data);
        //this_.m_is_open = false;;
        this_.m_port = 0;
        this_.m_ipv4_addr = 0;
    };

    delete reinterpret_cast<uv_tcp_t*>(handle);
}

} // namespace io

#include "TcpConnectedClient.h"

#include "ByteSwap.h"
#include "TcpServer.h"

#include <assert.h>

// TODO: move
#include <iostream>

namespace io {

TcpConnectedClient::TcpConnectedClient(EventLoop& loop, TcpServer& server) :
    Disposable(loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())),
    m_is_open(true),
    m_server(&server) {
    init_stream();
}

TcpConnectedClient::~TcpConnectedClient() {
    IO_LOG(m_loop, TRACE, this, "_");

    if (m_read_buf) {
        delete[] m_read_buf;
    }
}

void TcpConnectedClient::init_stream() {
    if (m_tcp_stream == nullptr) {
        m_tcp_stream = new uv_tcp_t;
        uv_tcp_init(m_uv_loop, m_tcp_stream);
        m_tcp_stream->data = this;
    }
}

std::uint32_t TcpConnectedClient::ipv4_addr() const {
    return m_ipv4_addr;
}

std::uint16_t TcpConnectedClient::port() const {
    return m_port;
}

void TcpConnectedClient::set_ipv4_addr(std::uint32_t value) {
    m_ipv4_addr = value;
}

void TcpConnectedClient::set_port(std::uint16_t value) {
    m_port = value;
}

void TcpConnectedClient::shutdown() {
    if (!is_open()) {
        return;
    }

    auto shutdown_req = new uv_shutdown_t;
    shutdown_req->data = this;
    uv_shutdown(shutdown_req, reinterpret_cast<uv_stream_t*>(m_tcp_stream), TcpConnectedClient::on_shutdown);
}

namespace {

struct WriteRequest : public uv_write_t {
    uv_buf_t uv_buf;
    std::shared_ptr<const char> buf;
    TcpConnectedClient::EndSendCallback end_send_callback = nullptr;
};

} // namespace

void TcpConnectedClient::send_data(std::shared_ptr<const char> buffer, std::size_t size, EndSendCallback callback) {
    auto req = new WriteRequest;
    req->end_send_callback = callback;
    req->data = this;
    req->buf = buffer;
    // const_cast is a workaround for lack of constness support in uv_buf_t
    req->uv_buf = uv_buf_init(const_cast<char*>(buffer.get()), size);

    int uv_code = uv_write(req, reinterpret_cast<uv_stream_t*>(m_tcp_stream), &req->uv_buf, 1, TcpConnectedClient::after_write);
    if (uv_code < 0) {
        std::cout << "!!! " << uv_strerror(uv_code) << std::endl;
    }
    ++m_pending_write_requesets;
}

void TcpConnectedClient::send_data(const std::string& message, EndSendCallback callback) {
    std::shared_ptr<char> ptr(new char[message.size()], [](const char* p) { delete[] p;});
    std::copy(message.c_str(), message.c_str() + message.size(), ptr.get());
    send_data(ptr, message.size(), callback);
}

void TcpConnectedClient::set_close_callback(CloseCallback callback) {
    m_close_callback = callback;
}

std::size_t TcpConnectedClient::pending_write_requesets() const {
    return m_pending_write_requesets;
}

uv_tcp_t* TcpConnectedClient::tcp_client_stream() {
    return m_tcp_stream;
}

void TcpConnectedClient::schedule_removal() {
    IO_LOG(m_loop, TRACE, "address:", io::ip4_addr_to_string(m_ipv4_addr), ":", port());

    m_server = nullptr;
    close();

    Disposable::schedule_removal();
}

void TcpConnectedClient::close() {
    if (!is_open()) {
        return;
    }

    IO_LOG(m_loop, TRACE, "address:", io::ip4_addr_to_string(m_ipv4_addr), ":", port());

    m_is_open = false;

    if (m_close_callback) {
        m_close_callback(*this, Status(0));
    }

    if (m_server) {
        m_server->remove_client_connection(this);
    }

    if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(m_tcp_stream))) {
        uv_close(reinterpret_cast<uv_handle_t*>(m_tcp_stream), TcpConnectedClient::on_close);
        m_tcp_stream->data = nullptr;
        m_tcp_stream = nullptr;
    }
}

bool TcpConnectedClient::is_open() const {
    return m_is_open;
}

void TcpConnectedClient::set_user_data(void* data)  {
    m_user_data = data;
}

void* TcpConnectedClient::user_data() {
    return m_user_data;
}

// ////////////////////////////////////// static //////////////////////////////////////
void TcpConnectedClient::after_write(uv_write_t* req, int status) {
    auto& this_ = *reinterpret_cast<TcpConnectedClient*>(req->data);

    assert(this_.m_pending_write_requesets >= 1);
    --this_.m_pending_write_requesets;

    auto request = reinterpret_cast<WriteRequest*>(req);
    std::unique_ptr<WriteRequest> guard(request);

    if (status < 0) {
        std::cerr << "!!! TcpConnectedClient::after_write: " << uv_strerror(status) << std::endl;
    }

    if (request->end_send_callback) {
        request->end_send_callback(this_);
    }
}

void TcpConnectedClient::on_shutdown(uv_shutdown_t* req, int status) {
    auto& this_ = *reinterpret_cast<TcpConnectedClient*>(req->data);

    IO_LOG(this_.m_loop, TRACE, "address:", io::ip4_addr_to_string(this_.m_ipv4_addr), ":", this_.port());

    // TODO: need close????
    //uv_close(reinterpret_cast<uv_handle_t*>(req->handle), TcpConnectedClient::on_close);
    delete req;
}

void TcpConnectedClient::start_read(DataReceiveCallback data_receive_callback) {
    m_receive_callback = data_receive_callback;

    if (m_receive_callback) {
        uv_read_start(reinterpret_cast<uv_stream_t*>(m_tcp_stream),
                      alloc_read_buffer,
                      on_read);
    }

    //TODO: set ip and port
    set_ipv4_addr(0);
    set_port(0);
}

void TcpConnectedClient::on_close(uv_handle_t* handle) {
    if (handle->data) {
        auto& this_ = *reinterpret_cast<TcpConnectedClient*>(handle->data);
        //this_.m_is_open = false;;
        this_.m_port = 0;
        this_.m_ipv4_addr = 0;
    };

    delete reinterpret_cast<uv_tcp_t*>(handle);
}

void TcpConnectedClient::on_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
    auto& this_ = *reinterpret_cast<TcpConnectedClient*>(handle->data);

    if (nread > 0) {
        if (this_.m_receive_callback) {
            this_.m_receive_callback(*this_.m_server, this_, buf->base, nread);
        }
        return;
        // TODO: not freeing buf->base
    }

    if (nread <= 0) {
        Status status(nread);

        if (status.code() == io::StatusCode::END_OF_FILE) {
            if (this_.m_close_callback) {
                this_.m_close_callback(this_, Status(0)); // OK
            }

            IO_LOG(this_.m_loop, TRACE, "connection end address:",
                              io::ip4_addr_to_string(this_.ipv4_addr()), ":", this_.port());
            this_.m_server->remove_client_connection(&this_);
        } else {
            //TODO: need to call read callback with error status here

            // Could be CONNECTION_RESET_BY_PEER (ECONNRESET), for example
            if (this_.m_close_callback) {
                this_.m_close_callback(this_, status);
            }
        }

        // TODO: uv_close on error????
    }

    //this_.m_pool->free(buf->base);
}

void TcpConnectedClient::alloc_read_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    auto& this_ = *reinterpret_cast<TcpConnectedClient*>(handle->data);

    if (this_.m_read_buf == nullptr) {
        io::default_alloc_buffer(handle, suggested_size, buf);
        this_.m_read_buf = buf->base;
        this_.m_read_buf_size = buf->len;
    } else {
        buf->base = this_.m_read_buf;
        buf->len = this_.m_read_buf_size;
    }
}

} // namespace io

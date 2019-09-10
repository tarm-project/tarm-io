#include "TcpClient.h"

#include "Common.h"
#include "ByteSwap.h"
#include "detail/TcpClientImplBase.h"

#include <assert.h>

namespace io {

class TcpClient::Impl : public detail::TcpClientImplBase<TcpClient, TcpClient::Impl> {
public:
    Impl(EventLoop& loop, TcpClient& parent);
    ~Impl();

    bool schedule_removal();

    void connect(const std::string& address,
                 std::uint16_t port,
                 ConnectCallback connect_callback,
                 DataReceiveCallback receive_callback,
                 CloseCallback close_callback = nullptr);
    void close();

    void set_close_callback(CloseCallback callback);

    void shutdown(CloseCallback callback);

protected:
    // statics
    static void on_shutdown(uv_shutdown_t* req, int uv_status);
    static void on_close(uv_handle_t* handle);
    static void on_connect(uv_connect_t* req, int uv_status);
    static void on_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf);

private:
    ConnectCallback m_connect_callback = nullptr;
    uv_connect_t* m_connect_req = nullptr;

    DataReceiveCallback m_receive_callback = nullptr;

    CloseCallback m_close_callback = nullptr;
};

TcpClient::Impl::Impl(EventLoop& loop, TcpClient& parent) :
    TcpClientImplBase(loop, parent) {
}

TcpClient::Impl::~Impl() {
    IO_LOG(m_loop, TRACE, this, "_");

    if (m_connect_req) {
        delete m_connect_req; // TODO: delete right after connect???
    }
}

void TcpClient::Impl::connect(const std::string& address,
                              std::uint16_t port,
                              ConnectCallback connect_callback,
                              DataReceiveCallback receive_callback,
                              CloseCallback close_callback) {
    struct sockaddr_in addr;

    uv_ip4_addr(address.c_str(), port, &addr); // TODO: error handling

    if (m_connect_req == nullptr) {
        m_connect_req = new uv_connect_t;
        m_connect_req->data = this;
    }

    m_port = port;
    m_ipv4_addr = network_to_host(addr.sin_addr.s_addr);

    IO_LOG(m_loop, DEBUG, "address:", io::ip4_addr_to_string(m_ipv4_addr)); // TODO: port???

    init_stream();
    m_connect_callback = connect_callback;
    m_receive_callback = receive_callback;
    m_close_callback = close_callback;

    int uv_status = uv_tcp_connect(m_connect_req, m_tcp_stream, reinterpret_cast<const struct sockaddr*>(&addr), on_connect);
    if (uv_status < 0) {
        Status status(uv_status);
        if (m_connect_callback) {
            m_connect_callback(*m_parent, status);
        }

        // TODO: if not close TcpClient handle, memory leak will occur
        //uv_close(reinterpret_cast<uv_handle_t*>(m_tcp_stream), on_close);
    }

}

void TcpClient::Impl::shutdown(CloseCallback callback) {
    if (!is_open()) {
        return;
    }

    m_close_callback = callback;

    auto shutdown_req = new uv_shutdown_t;
    shutdown_req->data = this;
    uv_shutdown(shutdown_req, reinterpret_cast<uv_stream_t*>(m_tcp_stream), on_shutdown);
}

void TcpClient::Impl::set_close_callback(CloseCallback callback) {
    m_close_callback = callback;
}

bool TcpClient::Impl::schedule_removal() {
    IO_LOG(m_loop, TRACE, "address:", io::ip4_addr_to_string(m_ipv4_addr), ":", port());

    close();

    return true;
}

void TcpClient::Impl::close() {
    if (!is_open()) {
        return;
    }

    IO_LOG(m_loop, TRACE, "address:", io::ip4_addr_to_string(m_ipv4_addr), ":", port());

    m_is_open = false;

    if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(m_tcp_stream))) {
        uv_close(reinterpret_cast<uv_handle_t*>(m_tcp_stream), on_close);
        m_tcp_stream->data = nullptr;
        m_tcp_stream = nullptr;
    }
}

////////////////////////////////////////////// static //////////////////////////////////////////////
void TcpClient::Impl::on_shutdown(uv_shutdown_t* req, int uv_status) {
    auto& this_ = *reinterpret_cast<TcpClient::Impl*>(req->data);

    IO_LOG(this_.m_loop, TRACE, "address:", io::ip4_addr_to_string(this_.m_ipv4_addr), ":", this_.port());

    Status status(uv_status);
    if (this_.m_close_callback && status.fail()) {
        this_.m_close_callback(*this_.m_parent, status);
    }

    this_.m_is_open = false;
    uv_close(reinterpret_cast<uv_handle_t*>(req->handle), on_close);

    delete req;
}

void TcpClient::Impl::on_connect(uv_connect_t* req, int uv_status) {
    auto& this_ = *reinterpret_cast<TcpClient::Impl*>(req->data);

    Status status(uv_status);
    this_.m_is_open = status.ok();

    if (this_.m_connect_callback) {
        this_.m_connect_callback(*this_.m_parent, status);
    }

    if (status.fail()) {
        this_.m_tcp_stream->data = nullptr;
        uv_close(reinterpret_cast<uv_handle_t*>(this_.m_tcp_stream), on_close);
        return;
    }

    if (this_.m_receive_callback) {
        uv_read_start(req->handle, alloc_read_buffer, on_read);
    }

    //TODO: set ip and port
    this_.set_ipv4_addr(0);
    this_.set_port(0);
}

void TcpClient::Impl::on_close(uv_handle_t* handle) {
    if (handle->data) {
        auto& this_ = *reinterpret_cast<TcpClient::Impl*>(handle->data);

        if (this_.m_close_callback) {
            this_.m_close_callback(*this_.m_parent, Status(0));
        }

        this_.m_port = 0;
        this_.m_ipv4_addr = 0;
    };

    delete reinterpret_cast<uv_tcp_t*>(handle);
}

void TcpClient::Impl::on_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
    auto& this_ = *reinterpret_cast<TcpClient::Impl*>(handle->data);

    Status status(nread);
    if (status.ok()) {
        if (this_.m_receive_callback) {
            this_.m_receive_callback(*this_.m_parent, buf->base,  nread);
        }
    } else {
        if (this_.m_close_callback) {
            this_.m_is_open = false;

            if (status.code() == io::StatusCode::END_OF_FILE) {
                this_.m_close_callback(*this_.m_parent, Status(0)); // OK
            } else {
                // Could be CONNECTION_RESET_BY_PEER (ECONNRESET), for example
                this_.m_close_callback(*this_.m_parent, status);
            }

            this_.m_tcp_stream->data = nullptr;
            uv_close(reinterpret_cast<uv_handle_t*>(this_.m_tcp_stream), on_close);
        }
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TcpClient::TcpClient(EventLoop& loop) :
    Disposable(loop),
    m_impl(new Impl(loop, *this)) {
}

TcpClient::~TcpClient() {
}

void TcpClient::schedule_removal() {
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Disposable::schedule_removal();
    }
}

std::uint32_t TcpClient::ipv4_addr() const {
    return m_impl->ipv4_addr();
}

std::uint16_t TcpClient::port() const {
    return m_impl->port();
}

void TcpClient::connect(const std::string& address,
                        std::uint16_t port,
                        ConnectCallback connect_callback,
                        DataReceiveCallback receive_callback,
                        CloseCallback close_callback) {
    return m_impl->connect(address, port, connect_callback, receive_callback, close_callback);
}

void TcpClient::close() {
    return m_impl->close();
}

bool TcpClient::is_open() const {
    return m_impl->is_open();
}

void TcpClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback) {
    return m_impl->send_data(buffer, size, callback);
}

void TcpClient::send_data(const std::string& message, EndSendCallback callback) {
    return m_impl->send_data(message, callback);
}

void TcpClient::set_close_callback(CloseCallback callback) {
    return m_impl->set_close_callback(callback);
}

std::size_t TcpClient::pending_write_requesets() const {
    return m_impl->pending_write_requesets();
}

void TcpClient::shutdown(CloseCallback callback) {
    return m_impl->shutdown(callback);
}

} // namespace io

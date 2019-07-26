#include "TcpClient.h"

#include "ByteSwap.h"
#include "TcpServer.h"

#include <assert.h>

// TODO: move
#include <iostream>

namespace io {

class TcpClient::Impl  {
public:
    Impl(EventLoop& loop, TcpClient& parent);
    ~Impl();

    bool schedule_removal();

    std::uint32_t ipv4_addr() const;
    std::uint16_t port() const;

    void connect(const std::string& address,
                 std::uint16_t port,
                 ConnectCallback connect_callback,
                 DataReceiveCallback receive_callback,
                 CloseCallback close_callback = nullptr);
    void close();

    bool is_open() const;

    void set_ipv4_addr(std::uint32_t value);
    void set_port(std::uint16_t value);

    void send_data(std::shared_ptr<const char> buffer, std::size_t size, EndSendCallback callback = nullptr);
    void send_data(const std::string& message, EndSendCallback callback = nullptr);

    void set_close_callback(CloseCallback callback);

    std::size_t pending_write_requesets() const;

    void shutdown();

protected:
    // statics
    static void after_write(uv_write_t* req, int status);
    static void alloc_read_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void on_shutdown(uv_shutdown_t* req, int status);
    static void on_close(uv_handle_t* handle);
    static void on_connect(uv_connect_t* req, int status);
    static void on_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf);

private:
    void init_stream();

    TcpClient* m_parent;
    EventLoop* m_loop;
    uv_loop_t* m_uv_loop;

    ConnectCallback m_connect_callback = nullptr;
    uv_connect_t* m_connect_req = nullptr;

    DataReceiveCallback m_receive_callback = nullptr;

    // TODO: need to ensure that one buffer is enough
    char* m_read_buf = nullptr;
    std::size_t m_read_buf_size = 0;

    bool m_is_open = false;

    uv_tcp_t* m_tcp_stream = nullptr;
    std::uint32_t m_ipv4_addr = 0;
    std::uint16_t m_port = 0;

    std::size_t m_pending_write_requesets = 0;

    CloseCallback m_close_callback = nullptr;
};


TcpClient::Impl::Impl(EventLoop& loop, TcpClient& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())) {
}

TcpClient::Impl::~Impl() {
    IO_LOG(m_loop, TRACE, this, "_");

    if (m_connect_req) {
        delete m_connect_req; // TODO: delete right after connect???
    }

    if (m_read_buf) {
        delete[] m_read_buf;
    }
}

void TcpClient::Impl::init_stream() {
    if (m_tcp_stream == nullptr) {
        m_tcp_stream = new uv_tcp_t;
        uv_tcp_init(m_uv_loop, m_tcp_stream);
        m_tcp_stream->data = this;
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
            m_connect_callback(*m_parent, uv_status);
        }
    }

}

std::uint32_t TcpClient::Impl::ipv4_addr() const {
    return m_ipv4_addr;
}

std::uint16_t TcpClient::Impl::port() const {
    return m_port;
}

void TcpClient::Impl::set_ipv4_addr(std::uint32_t value) {
    m_ipv4_addr = value;
}

void TcpClient::Impl::set_port(std::uint16_t value) {
    m_port = value;
}

void TcpClient::Impl::shutdown() {
    if (!is_open()) {
        return;
    }

    auto shutdown_req = new uv_shutdown_t;
    shutdown_req->data = this;
    uv_shutdown(shutdown_req, reinterpret_cast<uv_stream_t*>(m_tcp_stream), on_shutdown);
}

namespace {

struct WriteRequest : public uv_write_t {
    uv_buf_t uv_buf;
    std::shared_ptr<const char> buf;
    TcpClient::EndSendCallback end_send_callback = nullptr;
};

} // namespace

void TcpClient::Impl::send_data(std::shared_ptr<const char> buffer, std::size_t size, EndSendCallback callback) {
    auto req = new WriteRequest;
    req->end_send_callback = callback;
    req->data = this;
    req->buf = buffer;
    // const_cast is a workaround for lack of constness support in uv_buf_t
    req->uv_buf = uv_buf_init(const_cast<char*>(buffer.get()), size);

    int uv_code = uv_write(req, reinterpret_cast<uv_stream_t*>(m_tcp_stream), &req->uv_buf, 1, after_write);
    if (uv_code < 0) {
        std::cout << "!!! " << uv_strerror(uv_code) << std::endl;
    }
    ++m_pending_write_requesets;
}

void TcpClient::Impl::send_data(const std::string& message, EndSendCallback callback) {
    std::shared_ptr<char> ptr(new char[message.size()], [](const char* p) { delete[] p;});
    std::copy(message.c_str(), message.c_str() + message.size(), ptr.get());
    send_data(ptr, message.size(), callback);
}

void TcpClient::Impl::set_close_callback(CloseCallback callback) {
    m_close_callback = callback;
}

std::size_t TcpClient::Impl::pending_write_requesets() const {
    return m_pending_write_requesets;
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

    if (m_close_callback) {
        m_close_callback(*m_parent, Status(0));
    }

    if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(m_tcp_stream))) {
        uv_close(reinterpret_cast<uv_handle_t*>(m_tcp_stream), on_close);
        m_tcp_stream->data = nullptr;
        m_tcp_stream = nullptr;
    }
}

bool TcpClient::Impl::is_open() const {
    return m_is_open;
}

////////////////////////////////////////////// static //////////////////////////////////////////////
void TcpClient::Impl::after_write(uv_write_t* req, int status) {
    auto& this_ = *reinterpret_cast<TcpClient::Impl*>(req->data);

    assert(this_.m_pending_write_requesets >= 1);
    --this_.m_pending_write_requesets;

    auto request = reinterpret_cast<WriteRequest*>(req);
    std::unique_ptr<WriteRequest> guard(request);

    if (status < 0) {
        std::cerr << "!!! TcpClient::after_write: " << uv_strerror(status) << std::endl;
    }

    if (request->end_send_callback) {
        request->end_send_callback(*this_.m_parent);
    }
}

void TcpClient::Impl::on_shutdown(uv_shutdown_t* req, int status) {
    auto& this_ = *reinterpret_cast<TcpClient::Impl*>(req->data);

    IO_LOG(this_.m_loop, TRACE, "address:", io::ip4_addr_to_string(this_.m_ipv4_addr), ":", this_.port());

    // TODO: need close????
    //uv_close(reinterpret_cast<uv_handle_t*>(req->handle), TcpClient::on_close);
    delete req;
}

void TcpClient::Impl::on_connect(uv_connect_t* req, int uv_status) {
    auto& this_ = *reinterpret_cast<TcpClient::Impl*>(req->data);
    this_.m_is_open = true; // if not error!

    Status status(uv_status);
    if (this_.m_connect_callback) {
        this_.m_connect_callback(*this_.m_parent, uv_status);
    }

    if (status.fail()) {
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
        //this_.m_is_open = false;;
        this_.m_port = 0;
        this_.m_ipv4_addr = 0;
    };

    delete reinterpret_cast<uv_tcp_t*>(handle);
}

void TcpClient::Impl::on_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
    auto& this_ = *reinterpret_cast<TcpClient::Impl*>(handle->data);

    if (nread > 0) {
        if (this_.m_receive_callback) {
            this_.m_receive_callback(*this_.m_parent, buf->base,  nread);
        }
    } else if (nread <= 0) {
        if (this_.m_close_callback) {
            Status status(nread);

            if (status.code() == io::StatusCode::END_OF_FILE) {
                this_.m_close_callback(*this_.m_parent, Status(0)); // OK
            } else {
                // Could be CONNECTION_RESET_BY_PEER (ECONNRESET), for example
                this_.m_close_callback(*this_.m_parent, status);

                //TODO: if close callback is not set (probably) need to call read callback with error status
            }
        }
    }
}

void TcpClient::Impl::alloc_read_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    auto& this_ = *reinterpret_cast<TcpClient::Impl*>(handle->data);

    if (this_.m_read_buf == nullptr) {
        io::default_alloc_buffer(handle, suggested_size, buf);
        this_.m_read_buf = buf->base;
        this_.m_read_buf_size = buf->len;
    } else {
        buf->base = this_.m_read_buf;
        buf->len = this_.m_read_buf_size;
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

void TcpClient::set_ipv4_addr(std::uint32_t value) {
    return m_impl->set_ipv4_addr(value);
}

void TcpClient::set_port(std::uint16_t value) {
    return m_impl->set_port(value);
}

void TcpClient::send_data(std::shared_ptr<const char> buffer, std::size_t size, EndSendCallback callback) {
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

void TcpClient::shutdown() {
    return m_impl->shutdown();
}

} // namespace io

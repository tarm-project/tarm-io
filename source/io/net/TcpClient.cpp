/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "net/TcpClient.h"

#include "detail/Common.h"
#include "detail/LibuvCompatibility.h"
#include "Convert.h"
#include "ByteSwap.h"
#include "detail/TcpClientImplBase.h"

#include <assert.h>
#include <iostream>

namespace tarm {
namespace io {
namespace net {

class TcpClient::Impl : public detail::TcpClientImplBase<TcpClient, TcpClient::Impl> {
public:
    Impl(EventLoop& loop, TcpClient& parent);
    ~Impl();

    bool schedule_removal();

    void connect(const Endpoint& endpoint,
                 const void* raw_endpoint,
                 const ConnectCallback& connect_callback,
                 const DataReceiveCallback& receive_callback,
                 const CloseCallback& close_callback);
    bool close();
    void close_with_reset();

    void shutdown();

    EventLoop* loop();

protected:
    void connect_impl(const Endpoint& endpoint,
                      const void* raw_endpoint,
                      const ConnectCallback& connect_callback,
                      const DataReceiveCallback& receive_callback,
                      const CloseCallback& close_callback);
    // statics
    static void on_shutdown(uv_shutdown_t* req, int uv_status);
    static void on_close(uv_handle_t* handle);
    static void on_connect(uv_connect_t* req, int uv_status);
    static void on_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf);

private:
    ConnectCallback m_connect_callback = nullptr;
    std::unique_ptr<uv_connect_t> m_connect_req;

    DataReceiveCallback m_receive_callback = nullptr;

    bool m_want_delete_object = false;
};

TcpClient::Impl::Impl(EventLoop& loop, TcpClient& parent) :
    TcpClientImplBase(loop, parent) {
}

TcpClient::Impl::~Impl() {
    IO_LOG(m_loop, TRACE, this, "");
}

EventLoop* TcpClient::Impl::loop() {
    return m_loop;
}

void TcpClient::Impl::connect(const Endpoint& endpoint,
                              const void* raw_endpoint,
                              const ConnectCallback& connect_callback,
                              const DataReceiveCallback& receive_callback,
                              const CloseCallback& close_callback) {
    if (m_tcp_stream) {
        m_tcp_stream->data = nullptr;
        uv_close(reinterpret_cast<uv_handle_t*>(m_tcp_stream), on_close);
        m_tcp_stream = nullptr;
    }

    m_loop->schedule_callback([=](EventLoop&) {
        const Endpoint e = endpoint;
        connect_impl(e, e.raw_endpoint(), connect_callback, receive_callback, close_callback);
    });
}

void TcpClient::Impl::connect_impl(const Endpoint& endpoint,
                                   const void* raw_endpoint,
                                   const ConnectCallback& connect_callback,
                                   const DataReceiveCallback& receive_callback,
                                   const CloseCallback& close_callback) {

    if (endpoint.type() == Endpoint::UNDEFINED) {
        if (connect_callback) {
            connect_callback(*m_parent, Error(StatusCode::INVALID_ARGUMENT));
        }
        return;
    }

    m_destination_endpoint = endpoint;

    const Error init_error = init_stream();
    if (init_error) {
        if (connect_callback) {
            connect_callback(*m_parent, Error(StatusCode::INVALID_ARGUMENT));
        }
        return;
    }

    m_connect_req.reset(new uv_connect_t);
    m_connect_req->data = this;

    auto addr = reinterpret_cast<const ::sockaddr_in*>(raw_endpoint);
    IO_LOG(m_loop, DEBUG, m_parent, "endpoint:", endpoint);

    m_connect_callback = connect_callback;
    m_receive_callback = receive_callback;
    m_close_callback = close_callback;

    const Error connect_error = uv_tcp_connect(m_connect_req.get(),
                                               m_tcp_stream,
                                               reinterpret_cast<const struct sockaddr*>(addr),
                                               on_connect);
    if (connect_error) {
        if (m_connect_callback) {
            m_connect_callback(*m_parent, connect_error);
        }
    }
}

void TcpClient::Impl::shutdown() {
    if (!is_open()) {
        return;
    }

    auto shutdown_req = new uv_shutdown_t;
    shutdown_req->data = this;
    uv_shutdown(shutdown_req, reinterpret_cast<uv_stream_t*>(m_tcp_stream), on_shutdown);
}

bool TcpClient::Impl::schedule_removal() {
    IO_LOG(m_loop, TRACE, m_parent, "endpoint:", m_destination_endpoint);

    m_want_delete_object = true;

    return close();
}

bool TcpClient::Impl::close() {
    if (!is_open()) {
        return true; // allow to remove object
    }

    IO_LOG(m_loop, TRACE, m_parent, "endpoint:", m_destination_endpoint);

    m_is_open = false;

    if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(m_tcp_stream))) {
        uv_close(reinterpret_cast<uv_handle_t*>(m_tcp_stream), on_close);
        m_tcp_stream = nullptr;
    }

    return false;
}

void TcpClient::Impl::close_with_reset() {
    if (!is_open()) {
        return;
    }

    IO_LOG(m_loop, TRACE, m_parent, "endpoint:", m_destination_endpoint);

    m_is_open = false;

    if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(m_tcp_stream))) {
        uv_tcp_close_reset(m_tcp_stream, on_close);
        m_tcp_stream = nullptr;
    }
}

////////////////////////////////////////////// static //////////////////////////////////////////////
void TcpClient::Impl::on_shutdown(uv_shutdown_t* req, int uv_status) {
    auto& this_ = *reinterpret_cast<TcpClient::Impl*>(req->data);

    IO_LOG(this_.m_loop, TRACE, this_.m_parent, "endpoint:", this_.m_destination_endpoint);

    Error error(uv_status);
    if (this_.m_close_callback && error) {
        this_.m_close_callback(*this_.m_parent, error);
    }

    this_.m_is_open = false;

    if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(this_.m_tcp_stream))) {
        uv_close(reinterpret_cast<uv_handle_t*>(req->handle), on_close);
    }

    delete req;
}

void TcpClient::Impl::on_connect(uv_connect_t* req, int uv_status) {
    auto& this_ = *reinterpret_cast<TcpClient::Impl*>(req->data);

    Error error(uv_status);
    this_.m_is_open = !error;

    if (this_.m_connect_callback) {
        this_.m_connect_callback(*this_.m_parent, error);
    }

    if (error && this_.m_tcp_stream) {
        this_.m_tcp_stream->data = nullptr;
        uv_close(reinterpret_cast<uv_handle_t*>(this_.m_tcp_stream), on_close);
        return;
    }

    uv_read_start(req->handle, alloc_read_buffer, on_read);
}

void TcpClient::Impl::on_close(uv_handle_t* handle) {
    auto loop_ptr = reinterpret_cast<EventLoop*>(handle->loop->data);
    IO_LOG(loop_ptr, TRACE, "");

    if (handle->data) {
        auto& this_ = *reinterpret_cast<TcpClient::Impl*>(handle->data);

        const bool should_delete = this_.m_want_delete_object;

        if (this_.m_close_callback) {
            this_.m_close_callback(*this_.m_parent, Error(0));
        }

        this_.m_destination_endpoint.clear();

        if (should_delete) {
            this_.m_parent->schedule_removal();
        }
    };

    delete reinterpret_cast<uv_tcp_t*>(handle);
}

void TcpClient::Impl::on_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
    auto& this_ = *reinterpret_cast<TcpClient::Impl*>(handle->data);
    auto& loop = *reinterpret_cast<EventLoop*>(handle->loop->data);

    this_.m_connect_req.reset();

    Error error(nread);
    if (!error) {
        if (nread && this_.m_receive_callback) {
            const auto prev_use_count = this_.m_read_buf.use_count();
            this_.m_receive_callback(*this_.m_parent, {this_.m_read_buf,  std::size_t(nread), this_.m_data_offset}, Error(0));
            if (prev_use_count != this_.m_read_buf.use_count()) { // user made a copy
                this_.m_read_buf.reset(); // will reallocate new one on demand
            }
        }

        this_.m_data_offset += static_cast<std::size_t>(nread);
    } else {
        IO_LOG(&loop, TRACE, "Closed from other side. Reason:", error.string());

        if (this_.m_close_callback) {
            this_.m_is_open = false;

            // Need this because user may connect to other endpoint in close callback
            auto old_tcp_stream = this_.m_tcp_stream;
            old_tcp_stream->data = nullptr;
            this_.m_tcp_stream = nullptr;

            this_.on_read_error(error);

            uv_close(reinterpret_cast<uv_handle_t*>(old_tcp_stream), on_close);
        }
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TcpClient::TcpClient(EventLoop& loop) :
    Removable(loop),
    m_impl(new Impl(loop, *this)) {
}

TcpClient::~TcpClient() {
}

void TcpClient::schedule_removal() {
    IO_LOG(m_impl->loop(), TRACE, this, "");

    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

const Endpoint& TcpClient::endpoint() const {
    return m_impl->endpoint();
}

void TcpClient::connect(const Endpoint& endpoint,
                        const ConnectCallback& connect_callback,
                        const DataReceiveCallback& receive_callback,
                        const CloseCallback& close_callback) {
    return m_impl->connect(endpoint, endpoint.raw_endpoint(), connect_callback, receive_callback, close_callback);
}

void TcpClient::close() {
    m_impl->close(); // returns bool
}

bool TcpClient::is_open() const {
    return m_impl->is_open();
}

void TcpClient::send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback)  {
    return m_impl->send_data(c_str, size, callback);
}

void TcpClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback) {
    return m_impl->send_data(buffer, size, callback);
}

void TcpClient::send_data(const std::string& message, const EndSendCallback& callback) {
    return m_impl->send_data(message, callback);
}

void TcpClient::send_data(std::string&& message, const EndSendCallback& callback) {
    return m_impl->send_data(std::move(message), callback);
}

std::size_t TcpClient::pending_send_requesets() const {
    return m_impl->pending_write_requests();
}

void TcpClient::shutdown() {
    return m_impl->shutdown();
}

void TcpClient::delay_send(bool enabled) {
    return m_impl->delay_send(enabled);
}

bool TcpClient::is_delay_send() const {
    return m_impl->is_delay_send();
}

void TcpClient::close_with_reset() {
    return m_impl->close_with_reset();
}

} // namespace net
} // namespace io
} // namespace tarm

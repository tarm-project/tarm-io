/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "EventLoop.h"
#include "detail/LogMacros.h"
#include "detail/RawBufferGetter.h"

#include <memory>
#include <assert.h>

namespace tarm {
namespace io {
namespace net {
namespace detail {

template<typename ParentType, typename ImplType>
class TcpClientImplBase {
public:
    TcpClientImplBase(AllocationContext& context, ParentType& parent);
    ~TcpClientImplBase();

    const Endpoint& endpoint() const;

    void copy_and_send_data(const char* c_str, std::uint32_t size, const typename ParentType::EndSendCallback& callback);
    void send_data(const char* c_str, std::uint32_t size, const typename ParentType::EndSendCallback& callback);
    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const typename ParentType::EndSendCallback& callback);
    void send_data(std::unique_ptr<char[]> buffer, std::uint32_t size, const typename ParentType::EndSendCallback& callback);
    void send_data(const std::string& message, const typename ParentType::EndSendCallback& callback);
    void send_data(std::string&& message, const typename ParentType::EndSendCallback& callback);

    std::size_t pending_write_requests() const;

    Error init_stream();

    bool is_open() const;

    void delay_send(bool enabled);
    bool is_delay_send() const;

    Error get_socket_error() const;

protected:
    template<typename T>
    void send_data_impl(T buffer, std::uint32_t size, const typename ParentType::EndSendCallback& callback);

    // statics
    template<typename T>
    static void after_write(uv_write_t* req, int status);
    static void alloc_read_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

    void on_read_error(const Error& error);

    // data
    EventLoop* m_loop;
    uv_loop_t* m_uv_loop;
    ParentType* m_parent;

    Endpoint m_destination_endpoint;

    uv_tcp_t* m_tcp_stream = nullptr;
    std::size_t m_pending_write_requests = 0;

    std::shared_ptr<char> m_read_buf;
    std::size_t m_read_buf_size = 0;

    std::size_t m_data_offset = 0;

    bool m_is_open = false;

    // This field added because libuv does not allow to get this property from TCP handle
    bool m_delay_send = true;

    typename ParentType::CloseCallback m_close_callback = nullptr;

private:
    template<typename T>
    struct WriteRequest : public uv_write_t {
        uv_buf_t uv_buf;
        typename ParentType::EndSendCallback end_send_callback;
        T buf;
    };
};

///////////////////////////////////////// implementation ///////////////////////////////////////////

template<typename ParentType, typename ImplType>
TcpClientImplBase<ParentType, ImplType>::TcpClientImplBase(AllocationContext& context, ParentType& parent) :
    m_loop(&context.loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(context.loop.raw_loop())),
    m_parent(&parent) {
}

template<typename ParentType, typename ImplType>
TcpClientImplBase<ParentType, ImplType>::~TcpClientImplBase() {
    m_read_buf.reset();
}

template<typename ParentType, typename ImplType>
const Endpoint& TcpClientImplBase<ParentType, ImplType>::endpoint() const {
    return m_destination_endpoint;
}

template<typename ParentType, typename ImplType>
Error TcpClientImplBase<ParentType, ImplType>::init_stream() {
    assert (m_tcp_stream == nullptr);

    m_tcp_stream = new uv_tcp_t;
    m_tcp_stream->data = this;

    Error init_error = uv_tcp_init(m_uv_loop, m_tcp_stream);
    if (init_error) {
        return init_error;
    }

    return Error(0);
}

template<typename ParentType, typename ImplType>
template<typename T>
void TcpClientImplBase<ParentType, ImplType>::send_data_impl(T buffer, std::uint32_t size, const typename ParentType::EndSendCallback& callback) {
    if (!is_open()) {
        if (callback) {
            callback(*m_parent, Error(StatusCode::NOT_CONNECTED));
        }
        return;
    }

    if (size == 0 || io::detail::raw_buffer_get(buffer) == nullptr) {
        if (callback) {
            callback(*m_parent, Error(StatusCode::INVALID_ARGUMENT));
        }
        return;
    }

    auto req = new WriteRequest<T>;
    req->end_send_callback = callback;
    req->data = this;
    req->buf = std::move(buffer);
    // const_cast is a workaround for lack of constness support in uv_buf_t
    req->uv_buf = uv_buf_init(const_cast<char*>(io::detail::raw_buffer_get(req->buf)), size);

    const Error write_error = uv_write(req, reinterpret_cast<uv_stream_t*>(m_tcp_stream), &req->uv_buf, 1, after_write<T>);
    if (write_error) {
        LOG_ERROR(m_loop, m_parent, "Error:", write_error.string());
        if (callback) {
            callback(*m_parent, write_error);
        }
        delete req;
        return;
    }

    ++m_pending_write_requests;
}

template<typename ParentType, typename ImplType>
void TcpClientImplBase<ParentType, ImplType>::copy_and_send_data(const char* c_str, std::uint32_t size, const typename ParentType::EndSendCallback& callback)  {
    std::unique_ptr<char[]> buf(new char [size]);
    std::memcpy(buf.get(), c_str, size);
    send_data(std::move(buf), size, callback);
}

template<typename ParentType, typename ImplType>
void TcpClientImplBase<ParentType, ImplType>::send_data(const char* c_str, std::uint32_t size, const typename ParentType::EndSendCallback& callback)  {
    send_data_impl(c_str, size, callback);
}

template<typename ParentType, typename ImplType>
void TcpClientImplBase<ParentType, ImplType>::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const typename ParentType::EndSendCallback& callback) {

    send_data_impl(buffer, size, callback);
}

template<typename ParentType, typename ImplType>
void TcpClientImplBase<ParentType, ImplType>::send_data(std::unique_ptr<char[]> buffer, std::uint32_t size, const typename ParentType::EndSendCallback& callback) {

    send_data_impl(std::move(buffer), size, callback);
}

template<typename ParentType, typename ImplType>
void TcpClientImplBase<ParentType, ImplType>::send_data(const std::string& message, const typename ParentType::EndSendCallback& callback) {
    std::shared_ptr<char> ptr(new char[message.size()], [](const char* p) { delete[] p;});
    std::copy(message.c_str(), message.c_str() + message.size(), ptr.get());
    send_data(ptr, static_cast<std::uint32_t>(message.size()), callback);
}

template<typename ParentType, typename ImplType>
void TcpClientImplBase<ParentType, ImplType>::send_data(std::string&& message, const typename ParentType::EndSendCallback& callback) {
    const std::uint32_t size = static_cast<std::uint32_t>(message.size());
    send_data_impl(std::move(message), size, callback);
}

template<typename ParentType, typename ImplType>
std::size_t TcpClientImplBase<ParentType, ImplType>::pending_write_requests() const {
    return m_pending_write_requests;
}

template<typename ParentType, typename ImplType>
bool TcpClientImplBase<ParentType, ImplType>::is_open() const {
    return m_is_open;
}

template<typename ParentType, typename ImplType>
void TcpClientImplBase<ParentType, ImplType>::delay_send(bool enabled) {
    uv_tcp_nodelay(m_tcp_stream, !enabled);
    m_delay_send = enabled;
}

template<typename ParentType, typename ImplType>
bool TcpClientImplBase<ParentType, ImplType>::is_delay_send() const {
    return m_delay_send;
}

template<typename ParentType, typename ImplType>
void TcpClientImplBase<ParentType, ImplType>::on_read_error(const Error& error) {
    LOG_DEBUG(m_loop, m_parent, "Connection end", endpoint(), "reason:", error);

    if (m_close_callback) {
        if (error.code() == StatusCode::END_OF_FILE) {
            m_close_callback(*m_parent, Error(0)); // OK
        } else {
            // Could be CONNECTION_RESET_BY_PEER (ECONNRESET), for example
            // TODO: is it possible to get non-fatal read error? So close callback should not be called here.
            m_close_callback(*m_parent, error);
        }
    }
}

template<typename ParentType, typename ImplType>
Error TcpClientImplBase<ParentType, ImplType>::get_socket_error() const {
    int value = 0;
    const Error getter_error = tarm_io_socket_option(reinterpret_cast<uv_handle_t*>(m_tcp_stream), SO_ERROR, &value);
    if (getter_error) {
        return getter_error;
    }

    return uv_translate_sys_error(value);
}

////////////////////////////////////////////// static //////////////////////////////////////////////
template<typename ParentType, typename ImplType>
template<typename T>
void TcpClientImplBase<ParentType, ImplType>::after_write(uv_write_t* req, int uv_status) {
    auto& this_ = *reinterpret_cast<ImplType*>(req->data);

    assert(this_.m_pending_write_requests >= 1);
    --this_.m_pending_write_requests;

    auto request = reinterpret_cast<WriteRequest<T>*>(req);
    std::unique_ptr<WriteRequest<T>> guard(request);

    Error error(uv_status);
    if (error) {
        LOG_ERROR(this_.m_loop, this_.m_parent, "Error:", uv_strerror(uv_status));
    }

    if (request->end_send_callback) {
        request->end_send_callback(*this_.m_parent, error);
    }
}

template<typename ParentType, typename ImplType>
void TcpClientImplBase<ParentType, ImplType>::alloc_read_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    auto& this_ = *reinterpret_cast<ImplType*>(handle->data);

    if (this_.m_read_buf == nullptr) {
        io::detail::default_alloc_buffer(handle, suggested_size, buf);

        this_.m_read_buf.reset(buf->base, std::default_delete<char[]>());
        this_.m_read_buf_size = buf->len;
    } else {
        buf->base =  this_.m_read_buf.get();
        buf->len = static_cast<decltype(uv_buf_t::len)>(this_.m_read_buf_size);
    }
}

} // namespace detail
} // namespace net
} // namespace io
} // namespace tarm

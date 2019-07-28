#pragma once

#include <uv.h>
#include <memory>

namespace io {
namespace detail {

template<typename ParentType, typename ImplType>
class TcpClientImplBase {
public:
    TcpClientImplBase(EventLoop& loop, ParentType& parent) :
        m_loop(&loop),
        m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())),
        m_parent(&parent) {
    }

    void send_data(std::shared_ptr<const char> buffer, std::size_t size, typename ParentType::EndSendCallback callback);
    void send_data(const std::string& message, typename ParentType::EndSendCallback callback);

protected:
    // statics
    static void after_write(uv_write_t* req, int status);

    // data
    EventLoop* m_loop;
    uv_loop_t* m_uv_loop;
    ParentType* m_parent;

    uv_tcp_t* m_tcp_stream = nullptr;
    std::size_t m_pending_write_requesets = 0;

private:
    struct WriteRequest : public uv_write_t {
        uv_buf_t uv_buf;
        std::shared_ptr<const char> buf;
        typename ParentType::EndSendCallback end_send_callback = nullptr;
    };
};

///////////////////////////////////////// implementation ///////////////////////////////////////////

template<typename ParentType, typename ImplType>
void TcpClientImplBase<ParentType, ImplType>::send_data(std::shared_ptr<const char> buffer, std::size_t size, typename ParentType::EndSendCallback callback) {
    auto req = new WriteRequest;
    req->end_send_callback = callback;
    req->data = this;
    req->buf = buffer;
    // const_cast is a workaround for lack of constness support in uv_buf_t
    req->uv_buf = uv_buf_init(const_cast<char*>(buffer.get()), size);

    int uv_code = uv_write(req, reinterpret_cast<uv_stream_t*>(m_tcp_stream), &req->uv_buf, 1, after_write);
    Status status(uv_code);

    if (status.fail()) {
        IO_LOG(m_loop, ERROR, m_parent, "Error:", uv_strerror(uv_code));
        if (callback) {
            callback(*m_parent, status);
        }
        delete req;
        return;
    }

    ++m_pending_write_requesets;
}

template<typename ParentType, typename ImplType>
void TcpClientImplBase<ParentType, ImplType>::send_data(const std::string& message, typename ParentType::EndSendCallback callback) {
    std::shared_ptr<char> ptr(new char[message.size()], [](const char* p) { delete[] p;});
    std::copy(message.c_str(), message.c_str() + message.size(), ptr.get());
    send_data(ptr, message.size(), callback);
}

////////////////////////////////////////////// static //////////////////////////////////////////////
template<typename ParentType, typename ImplType>
void TcpClientImplBase<ParentType, ImplType>::after_write(uv_write_t* req, int uv_status) {
    auto& this_ = *reinterpret_cast<ImplType*>(req->data);

    assert(this_.m_pending_write_requesets >= 1);
    --this_.m_pending_write_requesets;

    auto request = reinterpret_cast<WriteRequest*>(req);
    std::unique_ptr<WriteRequest> guard(request);

    Status status(uv_status);
    if (status.fail()) {
        IO_LOG(this_.m_loop, ERROR, this_.m_parent, "Error:", uv_strerror(uv_status));
    }

    if (request->end_send_callback) {
        request->end_send_callback(*this_.m_parent, status);
    }
}

} // namespace detail
} // namespace io

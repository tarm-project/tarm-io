#pragma once

#include "io/EventLoop.h"

// TODO: remove???
#include <uv.h>

#include <cstring>

namespace io {
namespace detail {

// TODO: do we need ImplType??? Looks like can replace with 'using ImplType = ParentType::Impl'
template<typename ParentType, typename ImplType>
class UdpImplBase {
public:
    UdpImplBase(EventLoop& loop, ParentType& parent);

    void send_data(const std::string& message, typename ParentType::EndSendCallback  callback);
    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, typename ParentType::EndSendCallback  callback);

protected:
    struct SendRequest : public uv_udp_send_t {
        uv_buf_t uv_buf;
        std::shared_ptr<const char> buf;
        typename ParentType::EndSendCallback end_send_callback = nullptr;
    };

    // statics
    static void on_send(uv_udp_send_t* req, int status);
    static void on_close_with_removal(uv_handle_t* handle);

    EventLoop* m_loop = nullptr;
    uv_loop_t* m_uv_loop = nullptr;
    ParentType* m_parent = nullptr;

    uv_udp_t m_udp_handle;
    sockaddr_storage m_raw_unix_addr;
};

///////////////////////////////////////// implementation ///////////////////////////////////////////

template<typename ParentType, typename ImplType>
UdpImplBase<ParentType, ImplType>::UdpImplBase(EventLoop& loop, ParentType& parent) :
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())),
    m_parent(&parent) {
    std::memset(&m_raw_unix_addr, 0, sizeof(m_raw_unix_addr));

    auto status = uv_udp_init(m_uv_loop, &m_udp_handle);
    if (status < 0) {
        // TODO: check status
    }

    m_udp_handle.data = this;
}

template<typename ParentType, typename ImplType>
void UdpImplBase<ParentType, ImplType>::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, typename ParentType::EndSendCallback callback) {
    auto req = new SendRequest;
    req->end_send_callback = callback;
    req->buf = buffer;
    // const_cast is a workaround for lack of constness support in uv_buf_t
    req->uv_buf = uv_buf_init(const_cast<char*>(buffer.get()), size);

    int uv_status = uv_udp_send(req, &m_udp_handle, &req->uv_buf, 1, reinterpret_cast<const sockaddr*>(&m_raw_unix_addr), on_send);
    if (uv_status < 0) {

    }
}

template<typename ParentType, typename ImplType>
void UdpImplBase<ParentType, ImplType>::send_data(const std::string& message, typename ParentType::EndSendCallback callback) {
    std::shared_ptr<char> ptr(new char[message.size()], [](const char* p) { delete[] p;});
    std::memcpy(ptr.get(), message.c_str(), message.size());
    send_data(ptr, static_cast<std::uint32_t>(message.size()), callback);
}

template<typename ParentType, typename ImplType>
void UdpImplBase<ParentType, ImplType>::on_send(uv_udp_send_t* req, int uv_status) {
    auto& request = *reinterpret_cast<SendRequest*>(req);
    auto& this_ = *reinterpret_cast<ImplType*>(req->handle->data);
    auto& parent = *this_.m_parent;

    Error error(uv_status);
    if (request.end_send_callback) {
        request.end_send_callback(parent, error);
    }

    delete &request;
}

template<typename ParentType, typename ImplType>
void UdpImplBase<ParentType, ImplType>::on_close_with_removal(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<ImplType*>(handle->data);
    auto& parent = *this_.m_parent;

    handle->data = nullptr;
    parent.schedule_removal();
}

} // namespace detail
} // namespace io

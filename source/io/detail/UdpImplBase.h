#pragma once

#include "io/EventLoop.h"

// TODO: remove???
#include <uv.h>

#include <cstring>
#include <memory>

namespace io {
namespace detail {

// TODO: do we need ImplType??? Looks like can replace with 'using ImplType = ParentType::Impl'
template<typename ParentType, typename ImplType>
class UdpImplBase {
public:
    UdpImplBase(EventLoop& loop, ParentType& parent);
    UdpImplBase(EventLoop& loop, ParentType& parent, uv_udp_t* udp_handle);

protected:
    // statics
    static void on_close_with_removal(uv_handle_t* handle);

    EventLoop* m_loop = nullptr;
    uv_loop_t* m_uv_loop = nullptr;
    ParentType* m_parent = nullptr;

    std::unique_ptr<uv_udp_t, std::function<void(uv_udp_t*)>> m_udp_handle;
    sockaddr_storage m_raw_unix_addr;
};

///////////////////////////////////////// implementation ///////////////////////////////////////////

template<typename ParentType, typename ImplType>
UdpImplBase<ParentType, ImplType>::UdpImplBase(EventLoop& loop, ParentType& parent) :
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())),
    m_parent(&parent),
    m_udp_handle(new uv_udp_t, std::default_delete<uv_udp_t>()) {
    std::memset(&m_raw_unix_addr, 0, sizeof(m_raw_unix_addr));

    auto status = uv_udp_init(m_uv_loop, m_udp_handle.get());
    if (status < 0) {
        // TODO: check status
    }

    m_udp_handle->data = this;
}

template<typename ParentType, typename ImplType>
UdpImplBase<ParentType, ImplType>::UdpImplBase(EventLoop& loop, ParentType& parent, uv_udp_t* udp_handle) :
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())),
    m_parent(&parent),
    m_udp_handle(udp_handle, [](uv_udp_t*){}) {
        std::memset(&m_raw_unix_addr, 0, sizeof(m_raw_unix_addr));
        m_udp_handle->data = udp_handle->data;
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

#pragma once

#include "io/BufferSizeResult.h"
#include "io/Endpoint.h"
#include "io/EventLoop.h"

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

    bool is_open() const;

    void set_last_packet_time(std::uint64_t time);
    std::uint64_t last_packet_time() const;

    BufferSizeResult receive_buffer_size() const;
    BufferSizeResult send_buffer_size() const;

protected:
    // statics
    static void on_close_with_removal(uv_handle_t* handle);
    static void on_close(uv_handle_t* handle);

    EventLoop* m_loop = nullptr;
    uv_loop_t* m_uv_loop = nullptr;
    ParentType* m_parent = nullptr;

    std::unique_ptr<uv_udp_t, std::function<void(uv_udp_t*)>> m_udp_handle;
    Endpoint m_destination_endpoint;

private:
    std::uint64_t m_last_packet_time_ns = 0;
};

///////////////////////////////////////// implementation ///////////////////////////////////////////

template<typename ParentType, typename ImplType>
UdpImplBase<ParentType, ImplType>::UdpImplBase(EventLoop& loop, ParentType& parent) :
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())),
    m_parent(&parent),
    m_udp_handle(new uv_udp_t, std::default_delete<uv_udp_t>()) {

    m_udp_handle.get()->u.fd = 0;

    this->set_last_packet_time(::uv_hrtime());

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
        m_udp_handle->data = udp_handle->data;
        this->set_last_packet_time(::uv_hrtime());
}

template<typename ParentType, typename ImplType>
void UdpImplBase<ParentType, ImplType>::on_close_with_removal(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<ImplType*>(handle->data);
    auto& parent = *this_.m_parent;

    handle->data = nullptr;
    parent.schedule_removal();
}

template<typename ParentType, typename ImplType>
bool UdpImplBase<ParentType, ImplType>::is_open() const {
    return m_udp_handle ? !uv_is_closing(reinterpret_cast<uv_handle_t*>(m_udp_handle.get())) : false;
}

template<typename ParentType, typename ImplType>
void UdpImplBase<ParentType, ImplType>::set_last_packet_time(std::uint64_t time) {
    m_last_packet_time_ns = time;
}

template<typename ParentType, typename ImplType>
std::uint64_t UdpImplBase<ParentType, ImplType>::last_packet_time() const {
    return m_last_packet_time_ns;
}

template<typename ParentType, typename ImplType>
BufferSizeResult UdpImplBase<ParentType, ImplType>::receive_buffer_size() const {
    int receive_size = 0;
    const Error receive_buffer_size_error =
        uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(m_udp_handle.get()), &receive_size);
    return {receive_buffer_size_error, static_cast<std::size_t>(receive_size)};
}

template<typename ParentType, typename ImplType>
BufferSizeResult UdpImplBase<ParentType, ImplType>::send_buffer_size() const {
    int receive_size = 0;
    const Error receive_buffer_size_error =
        uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(m_udp_handle.get()), &receive_size);
    return {receive_buffer_size_error, static_cast<std::size_t>(receive_size)};
}

} // namespace detail
} // namespace io

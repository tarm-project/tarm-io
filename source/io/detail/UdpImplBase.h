#pragma once

#include "io/BufferSizeResult.h"
#include "io/Endpoint.h"
#include "io/EventLoop.h"

#include <uv.h>

#include <cstring>
#include <limits>
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

    Error set_receive_buffer_size(std::size_t size);
    Error set_send_buffer_size(std::size_t size);

protected:
    Error check_buffer_size_value(std::size_t size) const;

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
    const Error error = uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(m_udp_handle.get()), &receive_size);
    return {error, static_cast<std::size_t>(receive_size)};
}

template<typename ParentType, typename ImplType>
BufferSizeResult UdpImplBase<ParentType, ImplType>::send_buffer_size() const {
    int receive_size = 0;
    const Error error = uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(m_udp_handle.get()), &receive_size);
    return {error, static_cast<std::size_t>(receive_size)};
}

template<typename ParentType, typename ImplType>
Error UdpImplBase<ParentType, ImplType>::check_buffer_size_value(std::size_t size) const {
    if (size == 0) {
        return Error(StatusCode::INVALID_ARGUMENT);
    }

    if (size > std::numeric_limits<int>::max()) {
        return Error(StatusCode::INVALID_ARGUMENT);
    }

    if (m_udp_handle.get()->io_watcher.fd == -1) {
        return Error(StatusCode::ADDRESS_NOT_AVAILABLE);
    }

    return Error(0);
}

template<typename ParentType, typename ImplType>
Error UdpImplBase<ParentType, ImplType>::set_receive_buffer_size(std::size_t size) {
    auto parameter_error = check_buffer_size_value(size);
    if (parameter_error) {
        return parameter_error;
    }

    auto receive_size = static_cast<int>(size);
    Error error = uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(m_udp_handle.get()), &receive_size);
    if (error) {
        return error;
    }

    return Error(0);
}

template<typename ParentType, typename ImplType>
Error UdpImplBase<ParentType, ImplType>::set_send_buffer_size(std::size_t size) {
    auto parameter_error = check_buffer_size_value(size);
    if (parameter_error) {
        return parameter_error;
    }

    auto send_size = static_cast<int>(size);
    Error error = uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(m_udp_handle.get()), &send_size);
    if (error) {
        return error;
    }

    return Error(0);
}

} // namespace detail
} // namespace io

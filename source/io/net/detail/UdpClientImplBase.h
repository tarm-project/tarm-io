/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "RefCounted.h"
#include "detail/RawBufferGetter.h"

#include "UdpImplBase.h"

#include <iostream>

namespace tarm {
namespace io {
namespace net {
namespace detail {

template<typename ParentType, typename ImplType>
class UdpClientImplBase : public UdpImplBase<ParentType, ImplType> {
public:
    UdpClientImplBase(EventLoop& loop, ParentType& parent);
    UdpClientImplBase(EventLoop& loop, ParentType& parent, RefCounted& ref_counted, uv_udp_t* udp_handle);

    void send_data(const char* c_str, std::uint32_t size, const typename ParentType::EndSendCallback& callback);
    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const typename ParentType::EndSendCallback& callback);
    void send_data(const std::string& message, const typename ParentType::EndSendCallback& callback);
    void send_data(std::string&& message, const typename ParentType::EndSendCallback& callback);

    std::uint16_t bound_port() const;

protected:
    template<typename T>
    struct SendRequest : public uv_udp_send_t {
        uv_buf_t uv_buf;
        typename ParentType::EndSendCallback end_send_callback;
        T buf;
    };

    template<typename T>
    void send_data_impl(T buffer, std::uint32_t size, const typename ParentType::EndSendCallback& callback);

    // statics
    template<typename T>
    static void on_send(uv_udp_send_t* req, int status);

    RefCounted* m_ref_counted = nullptr;

private:
    void schedule_send_error(const typename ParentType::EndSendCallback& callback, const Error& error) {
        if (callback) {
            auto parent = UdpImplBase<ParentType, ImplType>::m_parent; // Working around bug in GCC 4.8.4
            UdpImplBase<ParentType, ImplType>::m_loop->schedule_callback([=](EventLoop&){
                callback(*parent, error);
            });
        }
    }
};

template<typename ParentType, typename ImplType>
UdpClientImplBase<ParentType, ImplType>::UdpClientImplBase(EventLoop& loop, ParentType& parent) :
    UdpImplBase<ParentType, ImplType>(loop, parent) {
}

template<typename ParentType, typename ImplType>
UdpClientImplBase<ParentType, ImplType>::UdpClientImplBase(EventLoop& loop, ParentType& parent, RefCounted& ref_counted, uv_udp_t* udp_handle) :
    UdpImplBase<ParentType, ImplType>(loop, parent, udp_handle),
    m_ref_counted(&ref_counted) {
}

template<typename ParentType, typename ImplType>
template<typename T>
void UdpClientImplBase<ParentType, ImplType>::send_data_impl(T buffer, std::uint32_t size, const typename ParentType::EndSendCallback& callback) {
    if (size == 0) {
        schedule_send_error(callback, Error(StatusCode::INVALID_ARGUMENT));
        return;
    }

    if (!UdpImplBase<ParentType, ImplType>::is_open()) {
        schedule_send_error(callback, Error(StatusCode::OPERATION_CANCELED));
        return;
    }

    if (UdpImplBase<ParentType, ImplType>::m_destination_endpoint.type() == Endpoint::UNDEFINED) {
        schedule_send_error(callback, Error(StatusCode::DESTINATION_ADDRESS_REQUIRED));
        return;
    }

    this->set_last_packet_time(::uv_hrtime());

    auto req = new SendRequest<T>;
    req->end_send_callback = callback;
    req->buf = std::move(buffer);
    req->data = this;
    // const_cast is a workaround for lack of constness support in uv_buf_t
    req->uv_buf = uv_buf_init(const_cast<char*>(io::detail::raw_buffer_get(req->buf)), size);

    int uv_status = uv_udp_send(req,
                                UdpImplBase<ParentType, ImplType>::m_udp_handle.get(),
                                &req->uv_buf,
                                1,
                                reinterpret_cast<const sockaddr*>(UdpImplBase<ParentType, ImplType>::m_raw_endpoint),
                                on_send<T>);
    if (uv_status < 0) {
        if (callback) {
            callback(*UdpImplBase<ParentType, ImplType>::m_parent, Error(uv_status));
        }
    } else {
        if (m_ref_counted) {
            m_ref_counted->ref();
        }
    }
}

template<typename ParentType, typename ImplType>
void UdpClientImplBase<ParentType, ImplType>::send_data(const char* c_str, std::uint32_t size, const typename ParentType::EndSendCallback& callback)  {
    send_data_impl(c_str, size, callback);
}

template<typename ParentType, typename ImplType>
void UdpClientImplBase<ParentType, ImplType>::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const typename ParentType::EndSendCallback& callback) {

    send_data_impl(buffer, size, callback);
}

template<typename ParentType, typename ImplType>
void UdpClientImplBase<ParentType, ImplType>::send_data(const std::string& message, const typename ParentType::EndSendCallback& callback) {
    std::shared_ptr<char> ptr(new char[message.size()], [](const char* p) { delete[] p;});
    std::memcpy(ptr.get(), message.c_str(), message.size());
    send_data(ptr, static_cast<std::uint32_t>(message.size()), callback);
}

template<typename ParentType, typename ImplType>
void UdpClientImplBase<ParentType, ImplType>::send_data(std::string&& message, const typename ParentType::EndSendCallback& callback) {
    const std::uint32_t size = static_cast<std::uint32_t>(message.size());
    send_data_impl(std::move(message), size, callback);
}

template<typename ParentType, typename ImplType>
template<typename T>
void UdpClientImplBase<ParentType, ImplType>::on_send(uv_udp_send_t* req, int uv_status) {
    auto& request = *reinterpret_cast<SendRequest<T>*>(req);
    auto& this_ = *reinterpret_cast<ImplType*>(req->data);
    auto& parent = *this_.m_parent;

    Error error(uv_status);
    if (request.end_send_callback) {
        request.end_send_callback(parent, error);
    }

    if (this_.m_ref_counted) {
        this_.m_ref_counted->unref();
    }
    delete &request;
}

template<typename ParentType, typename ImplType>
std::uint16_t  UdpClientImplBase<ParentType, ImplType>::bound_port() const  {
    if (!UdpImplBase<ParentType, ImplType>::is_open()) {
        return 0;
    }

    sockaddr_storage address;
    int uv_size = sizeof(address);

    auto getsockname_status = uv_udp_getsockname(UdpImplBase<ParentType, ImplType>::m_udp_handle.get(), reinterpret_cast<sockaddr*>(&address), &uv_size);
    Error error(getsockname_status);
    if (error) {
        return 0;
    }

    auto& addr_in = *reinterpret_cast<sockaddr_in*>(&address);
    return network_to_host(addr_in.sin_port);
}

} // namespace detail
} // namespace net
} // namespace io
} // namespace tarm

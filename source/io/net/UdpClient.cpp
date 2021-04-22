/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "net/UdpClient.h"

#include "BacklogWithTimeout.h"
#include "ByteSwap.h"
#include "detail/UdpClientImplBase.h"

#include <cstring>
#include <cstddef>
#include <assert.h>

namespace tarm {
namespace io {
namespace net {

class UdpClient::Impl : public detail::UdpClientImplBase<UdpClient, UdpClient::Impl> {
public:
    Impl(AllocationContext& context, UdpClient& parent);
    Impl(AllocationContext& context, const Endpoint& endpoint, UdpClient& parent);
    ~Impl();

    using CloseHandler = void (*)(uv_handle_t* handle);
    void close_no_removal();
    bool close_with_removal();

    void set_destination(const Endpoint& endpoint,
                    const DestinationSetCallback& destination_set_callback,
                    const DataReceivedCallback& receive_callback);

    void set_destination(const Endpoint& endpoint,
                         const DestinationSetCallback& destination_set_callback,
                         const DataReceivedCallback& receive_callback,
                         std::size_t timeout_ms,
                         const CloseCallback& close_callback);

protected:
    Error start_receive();
    Error setup_udp_handle(const Endpoint& endpoint);
    bool close_impl(CloseHandler handler);

    Error set_destination_impl(const Endpoint& endpoint,
                               const DataReceivedCallback& receive_callback);

    // statics
    static void on_data_received(
        uv_udp_t* handle, ssize_t nread, const uv_buf_t* uv_buf, const struct sockaddr* addr, unsigned flags);

    static void on_close_no_removal(uv_handle_t* handle);

private:
    DataReceivedCallback m_receive_callback = nullptr;

    CloseCallback m_close_callback = nullptr;

    // Here is a bit unusual usage of backlog which consists of one single element to track expiration
    std::unique_ptr<BacklogWithTimeout<UdpClient::Impl*>> m_timeout_handler;
    std::function<void(BacklogWithTimeout<UdpClient::Impl*>&, UdpClient::Impl* const& )> m_on_item_expired = nullptr;
};

UdpClient::Impl::Impl(AllocationContext& context, UdpClient& parent) :
    UdpClientImplBase(context, parent) {

    m_on_item_expired = [this](BacklogWithTimeout<UdpClient::Impl*>&, UdpClient::Impl* const & item) {
        this->close_no_removal();
    };
}

Error UdpClient::Impl::set_destination_impl(const Endpoint& endpoint,
                                            const DataReceivedCallback& receive_callback) {
    Error destination_error = setup_udp_handle(endpoint);
    if (destination_error) {
        return destination_error;
    }

    const Error start_receive_error = start_receive();
    if (start_receive_error) {
        return start_receive_error;
    }

    m_receive_callback = receive_callback;
    return Error(0);
}

void  UdpClient::Impl::set_destination(const Endpoint& endpoint,
                                       const DestinationSetCallback& destination_set_callback,
                                       const DataReceivedCallback& receive_callback) {
    m_loop->schedule_callback([this, endpoint, destination_set_callback, receive_callback](EventLoop&) {
        if (m_parent->is_removal_scheduled()) {
            return;
        }

        auto error = set_destination_impl(endpoint, receive_callback);
        if (destination_set_callback) {
            destination_set_callback(*m_parent, error);
        }
    });
}

void UdpClient::Impl::set_destination(const Endpoint& endpoint,
                                      const DestinationSetCallback& destination_set_callback,
                                      const DataReceivedCallback& receive_callback,
                                      std::size_t timeout_ms,
                                      const CloseCallback& close_callback) {
    m_loop->schedule_callback([=](EventLoop&) {
        if (m_parent->is_removal_scheduled()) {
            return;
        }

        auto error = set_destination_impl(endpoint, receive_callback);
        if (destination_set_callback) {
            destination_set_callback(*m_parent, error);
        }

        m_close_callback = close_callback;
        m_timeout_handler.reset(new BacklogWithTimeout<UdpClient::Impl*>(*m_loop, timeout_ms, m_on_item_expired, std::bind(&UdpClient::Impl::last_packet_time, this), &::uv_hrtime));
        m_timeout_handler->add_item(this);
    });
}

UdpClient::Impl::~Impl() {
    LOG_TRACE(m_loop, m_parent, "Deleted UdpClient");
}

// TODO: use uv_udp_connect if libuv version is >= 1.27.0
Error UdpClient::Impl::setup_udp_handle(const Endpoint& endpoint) {
    const auto handle_init_error = ensure_handle_inited();
    if (handle_init_error) {
        return handle_init_error;
    }

    if (endpoint.type() == Endpoint::UNDEFINED) {
        return Error(StatusCode::INVALID_ARGUMENT);
    }

    if ((m_udp_handle.get()->flags & TARM_IO_UV_HANDLE_BOUND) == 0) {
        ::sockaddr_storage storage{0};
        storage.ss_family = endpoint.type() == Endpoint::IP_V4 ? AF_INET : AF_INET6;
        const Error bind_error = uv_udp_bind(m_udp_handle.get(), reinterpret_cast<const ::sockaddr*>(&storage), UV_UDP_REUSEADDR);
        if (bind_error) {
            return bind_error;
        }
    } else {
        if (m_destination_endpoint.type() != Endpoint::UNDEFINED &&
            m_destination_endpoint.type() != endpoint.type()) {
            return StatusCode::INVALID_ARGUMENT;
        }
    }

    m_destination_endpoint = endpoint;
    m_raw_endpoint = m_destination_endpoint.raw_endpoint();

    return Error(0);
}

bool UdpClient::Impl::close_with_removal() {
    return close_impl(&on_close_with_removal);
}

void UdpClient::Impl::close_no_removal() {
    close_impl(&on_close_no_removal);
}

bool UdpClient::Impl::close_impl(CloseHandler handler) {
    if (is_open()) {
        uv_udp_recv_stop(m_udp_handle.get());
        uv_close(reinterpret_cast<uv_handle_t*>(m_udp_handle.get()), handler);
        return false; // not ready to remove
    }

    return true;
}

Error UdpClient::Impl::start_receive() {
    Error recv_start_error = uv_udp_recv_start(m_udp_handle.get(), io::detail::default_alloc_buffer, on_data_received);
    if (recv_start_error) {
        return recv_start_error;
    }

    return Error(0);
}

///////////////////////////////////////////  static  ////////////////////////////////////////////

void UdpClient::Impl::on_data_received(uv_udp_t* handle,
                                       ssize_t nread,
                                       const uv_buf_t* uv_buf,
                                       const struct sockaddr* addr,
                                       unsigned flags) {
    assert(handle);
    auto& this_ = *reinterpret_cast<UdpClient::Impl*>(handle->data);
    auto& parent = *this_.m_parent;

    this_.set_last_packet_time(::uv_hrtime());

    std::shared_ptr<const char> buf(uv_buf->base, std::default_delete<char[]>());

    if (!this_.m_receive_callback) {
        return;
    }

    Error error(nread);

    if (!error) {
        if (addr && nread) {
            const auto& address_in_from = *reinterpret_cast<const struct sockaddr_in*>(addr);
            const auto& address_in_expect = *reinterpret_cast<sockaddr_in*>(this_.m_destination_endpoint.raw_endpoint());

            if (address_in_from.sin_addr.s_addr == address_in_expect.sin_addr.s_addr &&
                address_in_from.sin_port == address_in_expect.sin_port) {

                DataChunk data_chunk(buf, std::size_t(nread));
                this_.m_receive_callback(parent, data_chunk, error);
            }
        }
    } else {
        DataChunk data(nullptr, 0);
        this_.m_receive_callback(parent, data, error);
    }
}

void UdpClient::Impl::on_close_no_removal(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<UdpClient::Impl*>(handle->data);
    auto& parent = *this_.m_parent;

    this_.reset_udp_handle_state();

    if (this_.m_close_callback) {
        this_.m_close_callback(parent, Error(0));
    }
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpClient::UdpClient(AllocationContext& context) :
    Removable(context.loop),
    m_impl(new UdpClient::Impl(context, *this)) {
}

UdpClient::~UdpClient() {
}


void UdpClient::set_destination(const Endpoint& endpoint,
                                const DestinationSetCallback& destination_set_callback,
                                const DataReceivedCallback& receive_callback) {
    return m_impl->set_destination(endpoint, destination_set_callback, receive_callback);
}

void UdpClient::set_destination(const Endpoint& endpoint,
                                const DestinationSetCallback& destination_set_callback,
                                const DataReceivedCallback& receive_callback,
                                std::size_t timeout_ms,
                                const CloseCallback& close_callback) {
    return m_impl->set_destination(endpoint, destination_set_callback, receive_callback, timeout_ms, close_callback);
}

void UdpClient::send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback) {
    return m_impl->send_data(c_str, size, callback);
}

void UdpClient::send_data(const std::string& message, const EndSendCallback& callback) {
    return m_impl->send_data(message, callback);
}

void UdpClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback) {
    return m_impl->send_data(buffer, size, callback);
}

void UdpClient::send_data(std::string&& message, const EndSendCallback& callback) {
    return m_impl->send_data(std::move(message), callback);
}

void UdpClient::schedule_removal() {
    set_removal_scheduled();
    const bool ready_to_remove = m_impl->close_with_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

std::uint16_t UdpClient::bound_port() const {
    return m_impl->bound_port();
}

const Endpoint& UdpClient::endpoint() const {
    return m_impl->endpoint();
}

bool UdpClient::is_open() const {
    return m_impl->is_open();
}

BufferSizeResult UdpClient::receive_buffer_size() const {
    return m_impl->receive_buffer_size();
}

BufferSizeResult UdpClient::send_buffer_size() const {
    return m_impl->send_buffer_size();
}

Error UdpClient::set_receive_buffer_size(std::size_t size) {
    return m_impl->set_receive_buffer_size(size);
}

Error UdpClient::set_send_buffer_size(std::size_t size) {
    return m_impl->set_send_buffer_size(size);
}

void UdpClient::close() {
    return m_impl->close_no_removal();
}

} // namespace net
} // namespace io
} // namespace tarm

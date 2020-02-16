#include "UdpClient.h"

#include "BacklogWithTimeout.h"
#include "ByteSwap.h"
#include "detail/UdpClientImplBase.h"

#include "detail/Common.h"

#include <cstring>
#include <cstddef>
#include <assert.h>

namespace io {

class UdpClient::Impl : public detail::UdpClientImplBase<UdpClient, UdpClient::Impl> {
public:
    Impl(EventLoop& loop, UdpClient& parent);
    Impl(EventLoop& loop, std::uint32_t host, std::uint16_t port, UdpClient& parent);
    Impl(EventLoop& loop, DataReceivedCallback receive_callback, UdpClient& parent);
    Impl(EventLoop& loop, DataReceivedCallback receive_callback, std::size_t timeout_ms, TimeoutCallback timeout_callback, UdpClient& parent);
    Impl(EventLoop& loop, std::uint32_t host, std::uint16_t port, DataReceivedCallback receive_callback, UdpClient& parent);
    Impl(EventLoop& loop, std::uint32_t host, std::uint16_t port, DataReceivedCallback receive_callback, std::size_t timeout_ms, TimeoutCallback timeout_callback, UdpClient& parent);

    using CloseHandler = void (*)(uv_handle_t* handle);
    bool close_on_timeout();
    bool close_with_removal();
    bool close(CloseHandler handler);

    void set_destination(std::uint32_t host, std::uint16_t port);

    void start_receive();

    std::uint32_t ipv4_addr() const;
    std::uint16_t port() const;

protected:
    std::uint64_t last_packet_receive_time() const;

    // statics
    static void on_data_received(
        uv_udp_t* handle, ssize_t nread, const uv_buf_t* uv_buf, const struct sockaddr* addr, unsigned flags);

    static void on_close_on_timeout(uv_handle_t* handle);

private:
    DataReceivedCallback m_receive_callback = nullptr;

    TimeoutCallback m_timeout_callback = nullptr;

    // Here is a bit unusual usage of backlog which consists of one single element to track expiration
    std::unique_ptr<BacklogWithTimeout<UdpClient::Impl*>> m_timeout_handler;
    std::function<void(BacklogWithTimeout<UdpClient::Impl*>&, UdpClient::Impl* const& )> m_on_item_expired = nullptr;

    std::uint64_t m_last_packet_time = 0;
};

UdpClient::Impl::Impl(EventLoop& loop, UdpClient& parent) :
    UdpClientImplBase(loop, parent) {
    m_on_item_expired = [this](BacklogWithTimeout<UdpClient::Impl*>&, UdpClient::Impl* const & item) {
        this->close_on_timeout();
    };
    m_last_packet_time = ::uv_hrtime();
}

UdpClient::Impl::Impl(EventLoop& loop, std::uint32_t host, std::uint16_t port, UdpClient& parent) :
    Impl(loop, parent) {
    set_destination(host, port);
}

UdpClient::Impl::Impl(EventLoop& loop, DataReceivedCallback receive_callback, UdpClient& parent) :
    Impl(loop, parent) {
    m_receive_callback = receive_callback;
    start_receive();
}

UdpClient::Impl::Impl(EventLoop& loop, std::uint32_t host, std::uint16_t port, DataReceivedCallback receive_callback, UdpClient& parent) :
    Impl(loop, parent) {
    set_destination(host, port);
    m_receive_callback = receive_callback;
    start_receive();
}


UdpClient::Impl::Impl(EventLoop& loop, DataReceivedCallback receive_callback, std::size_t timeout_ms, TimeoutCallback timeout_callback, UdpClient& parent) :
    Impl(loop, parent) {
    m_receive_callback = receive_callback;
    m_timeout_callback = timeout_callback;
    m_timeout_handler.reset(new BacklogWithTimeout<UdpClient::Impl*>(loop, timeout_ms, m_on_item_expired, std::bind(&UdpClient::Impl::last_packet_receive_time, this), &::uv_hrtime));
    m_timeout_handler->add_item(this);
    start_receive();
}

UdpClient::Impl::Impl(EventLoop& loop, std::uint32_t host, std::uint16_t port, DataReceivedCallback receive_callback, std::size_t timeout_ms, TimeoutCallback timeout_callback, UdpClient& parent) :
    Impl(loop, parent) {
    set_destination(host, port);
    m_receive_callback = receive_callback;
    m_timeout_callback = timeout_callback;
    m_timeout_handler.reset(new BacklogWithTimeout<UdpClient::Impl*>(loop, timeout_ms, m_on_item_expired, std::bind(&UdpClient::Impl::last_packet_receive_time, this), &::uv_hrtime));
    m_timeout_handler->add_item(this);
    start_receive();
}

void UdpClient::Impl::set_destination(std::uint32_t host, std::uint16_t port) {
    auto& unix_addr = *reinterpret_cast<sockaddr_in*>(&m_destination_address);
    unix_addr.sin_family = AF_INET;
    unix_addr.sin_port = host_to_network(port);
    unix_addr.sin_addr.s_addr = host_to_network(host);
    //uv_ip4_addr(ip_addr_str.c_str(), port, &unix_addr);
}

bool UdpClient::Impl::close_with_removal() {
    return close(&on_close_with_removal);
}

bool UdpClient::Impl::close_on_timeout() {
    return close(&on_close_on_timeout);
}

bool UdpClient::Impl::close(CloseHandler handler) {
    if (is_open()) {
        if (m_receive_callback) {
            uv_udp_recv_stop(m_udp_handle.get());
        }

        uv_close(reinterpret_cast<uv_handle_t*>(m_udp_handle.get()), handler);
        return false; // not ready to remove
    }

    return true;
}

void UdpClient::Impl::start_receive() {
    int status = uv_udp_recv_start(m_udp_handle.get(), detail::default_alloc_buffer, on_data_received);
    if (status < 0) {
        // TODO: error handling
    }
}

std::uint32_t UdpClient::Impl::ipv4_addr() const {
    const auto& unix_addr = *reinterpret_cast<const sockaddr_in*>(&m_destination_address);
    return network_to_host(unix_addr.sin_addr.s_addr);
}

std::uint16_t UdpClient::Impl::port() const {
    const auto& unix_addr = *reinterpret_cast<const sockaddr_in*>(&m_destination_address);
    return network_to_host(unix_addr.sin_port);
}

std::uint64_t UdpClient::Impl::last_packet_receive_time() const {
    return m_last_packet_time;
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

    // TODO: update also on packet send???
    this_.m_last_packet_time = ::uv_hrtime();

    // TODO: need some mechanism to reuse memory
    std::shared_ptr<const char> buf(uv_buf->base, std::default_delete<char[]>());

    if (!this_.m_receive_callback) {
        return;
    }

    Error error(nread);

    if (!error) {
        if (addr && nread) {
            const auto& address_in_from = *reinterpret_cast<const struct sockaddr_in*>(addr);
            const auto& address_in_expect = *reinterpret_cast<sockaddr_in*>(&this_.m_destination_address);

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

void UdpClient::Impl::on_close_on_timeout(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<UdpClient::Impl*>(handle->data);
    auto& parent = *this_.m_parent;

    handle->data = nullptr;

    if (this_.m_timeout_callback) {
        this_.m_timeout_callback(parent, Error(0));
    }
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

IO_DEFINE_DEFAULT_MOVE(UdpClient);

UdpClient::UdpClient(EventLoop& loop) :
    Removable(loop),
    m_impl(new UdpClient::Impl(loop, *this)) {
}

UdpClient::UdpClient(EventLoop& loop, std::uint32_t dest_host, std::uint16_t dest_port) :
    Removable(loop),
    m_impl(new UdpClient::Impl(loop, dest_host, dest_port, *this)) {
}

UdpClient::UdpClient(EventLoop& loop, DataReceivedCallback receive_callback) :
    Removable(loop),
    m_impl(new UdpClient::Impl(loop, receive_callback, *this)) {
}

UdpClient::UdpClient(EventLoop& loop,
                     DataReceivedCallback receive_callback,
                     std::size_t timeout_ms,
                     TimeoutCallback timeout_callback) :
    Removable(loop),
    m_impl(new UdpClient::Impl(loop, receive_callback, timeout_ms, timeout_callback, *this)) {
}

UdpClient::UdpClient(EventLoop& loop,
                     std::uint32_t dest_host,
                     std::uint16_t dest_port,
                     DataReceivedCallback receive_callback) :
    Removable(loop),
    m_impl(new UdpClient::Impl(loop, dest_host, dest_port, receive_callback, *this)) {
}

UdpClient::UdpClient(EventLoop& loop,
                     std::uint32_t dest_host,
                     std::uint16_t dest_port,
                     DataReceivedCallback receive_callback,
                     std::size_t timeout_ms,
                     TimeoutCallback timeout_callback) :
    Removable(loop),
    m_impl(new UdpClient::Impl(loop, dest_host, dest_port, receive_callback, timeout_ms, timeout_callback, *this)) {
}


UdpClient::~UdpClient() {
}

void UdpClient::set_destination(std::uint32_t host, std::uint16_t port) {
    return m_impl->set_destination(host, port);
}

void UdpClient::send_data(const std::string& message, EndSendCallback callback) {
    return m_impl->send_data(message, callback);
}

void UdpClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback) {
    return m_impl->send_data(buffer, size, callback);
}

void UdpClient::schedule_removal() {
    const bool ready_to_remove = m_impl->close_with_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

std::uint16_t UdpClient::bound_port() const {
    return m_impl->bound_port();
}

std::uint32_t UdpClient::ipv4_addr() const {
    return m_impl->ipv4_addr();
}

std::uint16_t UdpClient::port() const {
    return m_impl->port();
}

bool UdpClient::is_open() const {
    return m_impl->is_open();
}

} // namespace io

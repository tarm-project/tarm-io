#include "UdpClient.h"

#include "detail/UdpImplBase.h"

#include "Common.h"

#include <cstring>
#include <cstddef>

namespace io {

class UdpClient::Impl : public detail::UdpImplBase<UdpClient, UdpClient::Impl> {
public:
    Impl(EventLoop& loop, UdpClient& parent);
    Impl(EventLoop& loop, std::uint32_t host, std::uint16_t port, UdpClient& parent);

    //void send_data(const std::string& message, EndSendCallback callback);
    //void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback);

    bool close_with_removal();

    void set_destination(std::uint32_t host, std::uint16_t port);

protected:
    //static void on_close_with_removal(uv_handle_t* handle);
};

UdpClient::Impl::Impl(EventLoop& loop, UdpClient& parent) :
    UdpImplBase(loop, parent) {
}

UdpClient::Impl::Impl(EventLoop& loop, std::uint32_t host, std::uint16_t port, UdpClient& parent) :
    Impl(loop, parent) {
    set_destination(host, port);
}

void UdpClient::Impl::set_destination(std::uint32_t host, std::uint16_t port) {
    auto& unix_addr = *reinterpret_cast<sockaddr_in*>(&m_raw_unix_addr);
    unix_addr.sin_family = AF_INET;
    unix_addr.sin_port = htons(port);
    unix_addr.sin_addr.s_addr = htonl(host);
    //uv_ip4_addr(ip_addr_str.c_str(), port, &unix_addr);
}

bool UdpClient::Impl::close_with_removal() {
    if (m_udp_handle.data) {
        uv_close(reinterpret_cast<uv_handle_t*>(&m_udp_handle), on_close_with_removal);
        return false; // not ready to remove
    }

    return true;
}


///////////////////////////////////////////  static  ////////////////////////////////////////////



/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpClient::UdpClient(EventLoop& loop) :
    Disposable(loop),
    m_impl(new UdpClient::Impl(loop, *this)) {
}

UdpClient::UdpClient(EventLoop& loop, std::uint32_t host, std::uint16_t port) :
    Disposable(loop),
    m_impl(new UdpClient::Impl(loop, host, port, *this)) {
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
        Disposable::schedule_removal();
    }
}

} // namespace io

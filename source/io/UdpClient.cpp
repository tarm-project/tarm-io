#include "UdpClient.h"

#include "Common.h"

#include <cstring>
#include <cstddef>

namespace io {

class UdpClient::Impl : public uv_udp_t {
public:
    Impl(EventLoop& loop, UdpClient& parent);

    void send_data(const std::string& message, std::uint32_t host, std::uint16_t port, EndSendCallback callback);
    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, std::uint32_t host, std::uint16_t port, EndSendCallback callback);

    bool close_with_removal();

protected:
    // statics
    static void on_send(uv_udp_send_t* req, int status);
    static void on_close_with_removal(uv_handle_t* handle);

private:
    EventLoop* m_loop = nullptr;
    uv_loop_t* m_uv_loop = nullptr;
};

UdpClient::Impl::Impl(EventLoop& loop, UdpClient& parent) :
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())) {
    auto status = uv_udp_init(m_uv_loop, this);
    if (status < 0) {
        // TODO: check status
    }

    this->data = &parent;
}

namespace {

struct SendRequest : public uv_udp_send_t {
    uv_buf_t uv_buf;
    std::shared_ptr<const char> buf;
    UdpClient::EndSendCallback end_send_callback = nullptr;
};

} // namespace

void UdpClient::Impl::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, std::uint32_t host, std::uint16_t port, EndSendCallback callback) {
    auto req = new SendRequest;
    req->end_send_callback = callback;
    req->data = this;
    req->buf = buffer;
    // const_cast is a workaround for lack of constness support in uv_buf_t
    req->uv_buf = uv_buf_init(const_cast<char*>(buffer.get()), size);

    sockaddr_storage raw_unix_addr;
    std::memset(&raw_unix_addr, 0, sizeof(sockaddr_storage));
    auto& unix_addr = *reinterpret_cast<sockaddr_in*>(&raw_unix_addr);
    unix_addr.sin_family = AF_INET;
    unix_addr.sin_port = htons(port);
    unix_addr.sin_addr.s_addr = htonl(host);
    //uv_ip4_addr(ip_addr_str.c_str(), port, &unix_addr);

    int uv_status = uv_udp_send(req, this, &req->uv_buf, 1, reinterpret_cast<const sockaddr*>(&raw_unix_addr), on_send);
    if (uv_status < 0) {

    }
}

void UdpClient::Impl::send_data(const std::string& message, std::uint32_t host, std::uint16_t port, EndSendCallback callback) {
    std::shared_ptr<char> ptr(new char[message.size()], [](const char* p) { delete[] p;});
    //std::copy(message.c_str(), message.c_str() + message.size(), ptr.get());
    std::memcpy(ptr.get(), message.c_str(), message.size());
    send_data(ptr, static_cast<std::uint32_t>(message.size()), host, port, callback);
}

bool UdpClient::Impl::close_with_removal() {
    if (this->data) {
        uv_close(reinterpret_cast<uv_handle_t*>(this), on_close_with_removal);
        return false; // not ready to remove
    }

    return true;
}

///////////////////////////////////////////  static  ////////////////////////////////////////////
void UdpClient::Impl::on_send(uv_udp_send_t* req, int uv_status) {
    auto& request = *reinterpret_cast<SendRequest*>(req);
    auto& this_ = *reinterpret_cast<UdpClient::Impl*>(req->data);
    auto& parent = *reinterpret_cast<UdpClient*>(this_.data);

    Status status(uv_status);
    if (request.end_send_callback) {
        request.end_send_callback(parent, status);
    }

    delete &request;
}

void UdpClient::Impl::on_close_with_removal(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<UdpClient::Impl*>(handle);
    auto& parent = *reinterpret_cast<UdpClient*>(this_.data);

    this_.data = nullptr;
    parent.schedule_removal();
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpClient::UdpClient(EventLoop& loop) :
    Disposable(loop),
    m_impl(new UdpClient::Impl(loop, *this)) {
}

UdpClient::~UdpClient() {
}

void UdpClient::send_data(const std::string& message, std::uint32_t host, std::uint16_t port, EndSendCallback callback) {
    return m_impl->send_data(message, host, port, callback);
}

void UdpClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, std::uint32_t host, std::uint16_t port, EndSendCallback callback) {
    return m_impl->send_data(buffer, size, host, port, callback);
}

void UdpClient::schedule_removal() {
    const bool ready_to_remove = m_impl->close_with_removal();
    if (ready_to_remove) {
        Disposable::schedule_removal();
    }
}

} // namespace io

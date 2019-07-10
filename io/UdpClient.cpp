#include "UdpClient.h"

namespace io {

class UdpClient::Impl : public uv_udp_t {
public:
    Impl(EventLoop& loop);

    void send_data(std::shared_ptr<const char> buffer, std::size_t size, std::uint32_t host, std::uint16_t port, EndSendCallback callback);

protected:
    // statics
    static void on_send(uv_udp_send_t* req, int status);

private:
    EventLoop* m_loop = nullptr;
    uv_loop_t* m_uv_loop = nullptr;
};

UdpClient::Impl::Impl(EventLoop& loop) :
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())) {
    auto status = uv_udp_init(m_uv_loop, this);
    if (status < 0) {
        // TODO: check status
    }
}

namespace {

struct SendRequest : public uv_udp_send_t {
    uv_buf_t uv_buf;
    std::shared_ptr<const char> buf;
    UdpClient::EndSendCallback end_send_callback = nullptr;
};

} // namespace

void UdpClient::Impl::send_data(std::shared_ptr<const char> buffer, std::size_t size, std::uint32_t host, std::uint16_t port, EndSendCallback callback) {
    auto req = new SendRequest;
    req->end_send_callback = callback;
    req->data = this;
    req->buf = buffer;
    // const_cast is a workaround for lack of constness support in uv_buf_t
    req->uv_buf = uv_buf_init(const_cast<char*>(buffer.get()), size);

    struct sockaddr_in unix_addr;
    unix_addr.sin_family = AF_INET;
    unix_addr.sin_port = htons(port);
    unix_addr.sin_addr.s_addr = htonl(host);
    //uv_ip4_addr(ip_addr_str.c_str(), port, &unix_addr);

    int status = uv_udp_send(req, this, &req->uv_buf, 1, reinterpret_cast<const sockaddr*>(&unix_addr), on_send);
    if (status < 0) {

    }
}

///////////////////////////////////////////  static  ////////////////////////////////////////////
void UdpClient::Impl::on_send(uv_udp_send_t* req, int status) {

}
/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpClient::UdpClient(EventLoop& loop) :
    Disposable(loop),
    m_impl(new UdpClient::Impl(loop)) {
}

UdpClient::~UdpClient() {
}

void UdpClient::send_data(std::shared_ptr<const char> buffer, std::size_t size, std::uint32_t host, std::uint16_t port, EndSendCallback callback) {
    return m_impl->send_data(buffer, size, host, port, callback);
}

} // namespace io

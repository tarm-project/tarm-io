#include "TcpClient.h"

// TODO: move
#include <iostream>

namespace io {

std::uint32_t TcpClient::ipv4_addr() const {
    return m_ipv4_addr;
}

std::uint16_t TcpClient::port() const {
    return m_port;
}

void TcpClient::set_ipv4_addr(std::uint32_t value) {
    m_ipv4_addr = value;
}

void TcpClient::set_port(std::uint16_t value) {
    m_port = value;
}

namespace {

struct WriteRequest : public uv_write_t {
    uv_buf_t buf;
};

} // namespace

void TcpClient::send_data(std::shared_ptr<char> buffer, std::size_t size) {
    auto req = new WriteRequest;
    req->buf = uv_buf_init(buffer.get(), size);

    uv_write(req, reinterpret_cast<uv_stream_t*>(this), &req->buf, 1, TcpClient::after_write);
}

void TcpClient::after_write(uv_write_t* req, int status) {
    auto request = reinterpret_cast<WriteRequest*>(req);
    delete request;

    if (status < 0) {
        std::cerr << "TcpClient::after_write: " << uv_strerror(status) << std::endl;
    }

    static int counter = 0;
    std::cout << "TcpClient::after_write: counter " << counter++ << std::endl;
}

} // namespace io
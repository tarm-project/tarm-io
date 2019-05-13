#pragma once

#include "Common.h"

#include <memory>

namespace io {

class TcpClient : public uv_tcp_t {
public:
    //TcpClient() = default;
    //~TcpClient() = default;

    std::uint32_t ipv4_addr() const;
    std::uint16_t port() const;

    void set_ipv4_addr(std::uint32_t value);
    void set_port(std::uint16_t value);

    void send_data(std::shared_ptr<char> buffer, std::size_t size);

    static void after_write(uv_write_t* req, int status);
private:
    std::uint32_t m_ipv4_addr;
    std::uint16_t m_port;
};

using TcpClientPtr = std::unique_ptr<TcpClient>;

} // namespace io
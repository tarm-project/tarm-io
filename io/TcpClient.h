#pragma once

#include "Common.h"

#include <memory>

namespace io {

class TcpClient : public uv_tcp_t {
public:
    using EndSendCallback = std::function<void(TcpClient&)>;

    //TcpClient() = default;
    //~TcpClient() = default;

    std::uint32_t ipv4_addr() const;
    std::uint16_t port() const;

    void set_ipv4_addr(std::uint32_t value);
    void set_port(std::uint16_t value);

    void send_data(const char* buffer, std::size_t size, EndSendCallback callback = nullptr);

    std::size_t pending_write_requesets() const;

    static void after_write(uv_write_t* req, int status);
private:
    std::uint32_t m_ipv4_addr;
    std::uint16_t m_port;

    std::size_t m_pending_write_requesets = 0;

    // TODO: probably end send callback should not be stored inside the client object but inside each request
    EndSendCallback m_end_send_callback = nullptr;
};

using TcpClientPtr = std::unique_ptr<TcpClient>;

} // namespace io
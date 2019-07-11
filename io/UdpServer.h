#pragma once

#include "EventLoop.h"
#include "Disposable.h"
#include "Status.h"

#include <memory>

namespace io {

// TODO: fwd include
class UdpClient;

class UdpServer : public Disposable {
public:
    using DataReceivedCallback = std::function<void(UdpServer&, std::uint32_t, std::uint16_t, const char*, std::size_t, const Status&)>;

    UdpServer(EventLoop& loop);

    UdpServer(const UdpServer& other) = delete;
    UdpServer& operator=(const UdpServer& other) = delete;

    UdpServer(UdpServer&& other) = default;
    UdpServer& operator=(UdpServer&& other) = default;

    // TODO: need to return some enum (or StatusCode) instead of int
    int bind(const std::string& ip_addr_str, std::uint16_t port);

    void start_receive(DataReceivedCallback receive_callback);

    void shutdown();
    void close();

protected:
    ~UdpServer();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
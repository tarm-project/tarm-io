#pragma once

#include "EventLoop.h"
#include "Disposable.h"

#include <memory>

namespace io {

class UdpClient : public Disposable {
public:
    using EndSendCallback = std::function<void(UdpClient&)>;

    UdpClient(EventLoop& loop);

    void send_data(std::shared_ptr<const char> buffer, std::size_t size, std::uint32_t host, std::uint16_t port, EndSendCallback callback = nullptr);

protected:
    ~UdpClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

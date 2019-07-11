#pragma once

#include "EventLoop.h"
#include "Disposable.h"
#include "Status.h"

#include <memory>

namespace io {

class UdpClient : public Disposable {
public:
    using EndSendCallback = std::function<void(UdpClient&, const Status&)>;

    UdpClient(EventLoop& loop);

    void send_data(std::shared_ptr<const char> buffer, std::size_t size, std::uint32_t host, std::uint16_t port, EndSendCallback callback = nullptr);
    void send_data(const std::string& message, std::uint32_t host, std::uint16_t port, EndSendCallback callback = nullptr);

    void schedule_removal() override;

protected:
    ~UdpClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

#pragma once

#include "EventLoop.h"
#include "Disposable.h"

#include <memory>

namespace io {

class UdpClient : public Disposable {
public:
    UdpClient(EventLoop& loop);

protected:
    ~UdpClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

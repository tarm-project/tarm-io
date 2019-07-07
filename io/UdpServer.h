#pragma once

#include "EventLoop.h"
#include "Disposable.h"

#include <memory>

namespace io {

class UdpServer : public Disposable {
public:
    UdpServer(EventLoop& loop);

protected:
    ~UdpServer();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

#pragma once

#include "Disposable.h"
#include "EventLoop.h"
#include "Export.h"

#include <memory>

namespace io {

class TlsTcpServer : public Disposable {
public:
    TlsTcpServer(EventLoop& loop);

protected:
    ~TlsTcpServer();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

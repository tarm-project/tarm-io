#pragma once

#include "Disposable.h"
#include "EventLoop.h"
#include "Export.h"

#include <memory>

namespace io {

class TlsTcpConnectedClient : public Disposable {
public:
    TlsTcpConnectedClient(EventLoop& loop);

protected:
    ~TlsTcpConnectedClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io


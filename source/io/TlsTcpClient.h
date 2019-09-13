#pragma once

#include "Disposable.h"
#include "EventLoop.h"
#include "Export.h"

#include <memory>

namespace io {

class TlsTcpClient : public Disposable {
public:
    IO_DLL_PUBLIC TlsTcpClient(EventLoop& loop);

protected:
    IO_DLL_PUBLIC ~TlsTcpClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
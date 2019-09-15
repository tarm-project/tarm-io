#pragma once

#include "Disposable.h"
#include "EventLoop.h"
#include "Export.h"
#include "Forward.h"

#include <memory>

namespace io {

class TlsTcpConnectedClient : public Disposable {
public:
    friend class TlsTcpServer;

    using DataReceiveCallback = std::function<void(TlsTcpServer&, TlsTcpConnectedClient&, const char*, std::size_t)>;

protected:
    ~TlsTcpConnectedClient();

private:
    TlsTcpConnectedClient(EventLoop& loop, TlsTcpServer& tls_server, TcpConnectedClient& tcp_client);
    void set_data_receive_callback(DataReceiveCallback callback);
    void on_data_receive(const char* buf, std::size_t size);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io


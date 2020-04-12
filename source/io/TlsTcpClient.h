#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "Endpoint.h"
#include "EventLoop.h"
#include "Error.h"
#include "Export.h"
#include "Forward.h"
#include "Removable.h"
#include "TlsVersion.h"

#include <memory>

namespace io {

class TlsTcpClient : public Removable {
public:
    using UnderlyingClientType = TcpClient;

    using ConnectCallback = std::function<void(TlsTcpClient&, const Error&)>;
    using CloseCallback = std::function<void(TlsTcpClient&, const Error&)>;
    using EndSendCallback = std::function<void(TlsTcpClient&, const Error&)>;
    using DataReceiveCallback = std::function<void(TlsTcpClient&, const DataChunk&, const Error&)>;

    IO_FORBID_COPY(TlsTcpClient);
    IO_FORBID_MOVE(TlsTcpClient);

    IO_DLL_PUBLIC TlsTcpClient(EventLoop& loop, TlsVersionRange version_range = DEFAULT_TLS_VERSION_RANGE);

    IO_DLL_PUBLIC void schedule_removal() override;

    IO_DLL_PUBLIC const Endpoint& endpoint() const;

    IO_DLL_PUBLIC
    void connect(const Endpoint endpoint,
                 ConnectCallback connect_callback,
                 DataReceiveCallback receive_callback = nullptr,
                 CloseCallback close_callback = nullptr);
    IO_DLL_PUBLIC void close();

    IO_DLL_PUBLIC bool is_open() const;

    IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(const std::string& message, EndSendCallback callback = nullptr);

    IO_DLL_PUBLIC TlsVersion negotiated_tls_version() const;

protected:
    IO_DLL_PUBLIC ~TlsTcpClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

#pragma once

#include "Removable.h"
#include "DataChunk.h"
#include "EventLoop.h"
#include "Error.h"
#include "Export.h"
#include "Forward.h"
#include "TlsVersion.h"

#include <memory>

// TODO: revise this
namespace io {

using X509 = void;
using EVP_PKEY = void;

// TODO: now we can schedule removal of connected client from any callback
//       need to revise this. Probably just to use close/shutdown only.
class TlsTcpConnectedClient : public Removable {
public:
    friend class TlsTcpServer;

    using UnderlyingClientType = TcpConnectedClient;

    using DataReceiveCallback = std::function<void(TlsTcpConnectedClient&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(TlsTcpConnectedClient&, const Error&)>;
    using EndSendCallback = std::function<void(TlsTcpConnectedClient&, const Error&)>;

    using NewConnectionCallback = std::function<void(TlsTcpConnectedClient&, const Error&)>;

    IO_DLL_PUBLIC void close();
    IO_DLL_PUBLIC void shutdown();
    IO_DLL_PUBLIC bool is_open() const;

    IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(const std::string& message, EndSendCallback callback = nullptr);

    IO_DLL_PUBLIC TlsTcpServer& server();
    IO_DLL_PUBLIC const TlsTcpServer& server() const;

    IO_DLL_PUBLIC TlsVersion negotiated_tls_version() const;

protected:
    ~TlsTcpConnectedClient();

private:
    TlsTcpConnectedClient(EventLoop& loop, TlsTcpServer& tls_server, NewConnectionCallback new_connection_callback, X509* certificate, EVP_PKEY* private_key, TcpConnectedClient& tcp_client);
    void set_data_receive_callback(DataReceiveCallback callback);
    void on_data_receive(const char* buf, std::size_t size);
    Error init_ssl();

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io


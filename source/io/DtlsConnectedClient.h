#pragma once

#pragma once

#include "DataChunk.h"
#include "Removable.h"
#include "EventLoop.h"
#include "Error.h"
#include "Export.h"
#include "Forward.h"

#include <memory>

// TODO: revise this
namespace io {

using X509 = void;
using EVP_PKEY = void;

class DtlsConnectedClient : protected Removable {
public:
    friend class DtlsServer;

    using UnderlyingClientType = UdpPeer;

    using DataReceiveCallback = std::function<void(DtlsServer&, DtlsConnectedClient&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(DtlsConnectedClient&, const Error&)>;
    using EndSendCallback = std::function<void(DtlsConnectedClient&, const Error&)>;

    using NewConnectionCallback = std::function<void(DtlsServer&, DtlsConnectedClient&, const Error&)>;

    IO_DLL_PUBLIC void close();
    IO_DLL_PUBLIC void shutdown();
    IO_DLL_PUBLIC bool is_open() const;

    IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(const std::string& message, EndSendCallback callback = nullptr);

protected:
    ~DtlsConnectedClient();

private:
    DtlsConnectedClient(EventLoop& loop, DtlsServer& dtls_server, NewConnectionCallback new_connection_callback, X509* certificate, EVP_PKEY* private_key, UdpPeer& udp_peer);
    void set_data_receive_callback(DataReceiveCallback callback);
    void on_data_receive(const char* buf, std::size_t size);
    Error init_ssl();

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io



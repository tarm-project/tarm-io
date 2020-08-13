/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "net/DtlsVersion.h"
#include "net/Endpoint.h"
#include "EventLoop.h"
#include "Error.h"
#include "Export.h"
#include "Forward.h"
#include "Removable.h"

namespace tarm {
namespace io {
namespace net {

class DtlsClient : public Removable {
public:
    using UnderlyingClientType = UdpClient;

    using ConnectCallback = std::function<void(DtlsClient&, const Error&)>;
    using EndSendCallback = std::function<void(DtlsClient&, const Error&)>;
    using DataReceiveCallback = std::function<void(DtlsClient&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(DtlsClient&, const Error&)>;

    TARM_IO_FORBID_COPY(DtlsClient);
    TARM_IO_FORBID_MOVE(DtlsClient);

    TARM_IO_DLL_PUBLIC DtlsClient(EventLoop& loop, DtlsVersionRange version_range = DEFAULT_DTLS_VERSION_RANGE);

    TARM_IO_DLL_PUBLIC void schedule_removal() override;

    TARM_IO_DLL_PUBLIC const Endpoint& endpoint() const;

    TARM_IO_DLL_PUBLIC
    void connect(const Endpoint& endpoint,
                 const ConnectCallback& connect_callback,
                 const DataReceiveCallback& receive_callback = nullptr,
                 const CloseCallback& close_callback = nullptr,
                 std::size_t timeout_ms = 1000);
    TARM_IO_DLL_PUBLIC void close();

    TARM_IO_DLL_PUBLIC bool is_open() const;

    TARM_IO_DLL_PUBLIC void send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(const std::string& message, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(std::string&& message, const EndSendCallback& callback = nullptr);

    TARM_IO_DLL_PUBLIC DtlsVersion negotiated_dtls_version() const;

    // Returns 0 on error
    TARM_IO_DLL_PUBLIC std::uint16_t bound_port() const;

protected:
    TARM_IO_DLL_PUBLIC ~DtlsClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace net
} // namespace io
} // namespace tarm

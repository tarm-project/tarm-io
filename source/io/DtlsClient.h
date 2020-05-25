/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "DtlsVersion.h"
#include "Endpoint.h"
#include "EventLoop.h"
#include "Error.h"
#include "Export.h"
#include "Forward.h"
#include "Removable.h"

namespace tarm {
namespace io {

class DtlsClient : public Removable {
public:
    using UnderlyingClientType = UdpClient;

    using ConnectCallback = std::function<void(DtlsClient&, const Error&)>;
    using EndSendCallback = std::function<void(DtlsClient&, const Error&)>;
    using DataReceiveCallback = std::function<void(DtlsClient&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(DtlsClient&, const Error&)>;

    IO_FORBID_COPY(DtlsClient);
    IO_FORBID_MOVE(DtlsClient);

    IO_DLL_PUBLIC DtlsClient(EventLoop& loop, DtlsVersionRange version_range = DEFAULT_DTLS_VERSION_RANGE);

    IO_DLL_PUBLIC void schedule_removal() override;

    IO_DLL_PUBLIC const Endpoint& endpoint() const;

    IO_DLL_PUBLIC
    void connect(const Endpoint& endpoint,
                 ConnectCallback connect_callback,
                 DataReceiveCallback receive_callback = nullptr,
                 CloseCallback close_callback = nullptr,
                 std::size_t timeout_ms = 1000);
    IO_DLL_PUBLIC void close();

    IO_DLL_PUBLIC bool is_open() const;

    IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(const std::string& message, EndSendCallback callback = nullptr);

    IO_DLL_PUBLIC DtlsVersion negotiated_dtls_version() const;

    // Returns 0 on error
    IO_DLL_PUBLIC std::uint16_t bound_port() const;

protected:
    IO_DLL_PUBLIC ~DtlsClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
} // namespace tarm

/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "net/Endpoint.h"
#include "EventLoop.h"
#include "Error.h"
#include "Export.h"
#include "Forward.h"
#include "Removable.h"
#include "net/TlsVersion.h"

#include <memory>

namespace tarm {
namespace io {
namespace net {

class TlsClient : public Removable {
public:
    friend class ::tarm::io::Allocator;

    using UnderlyingClientType = TcpClient;

    using ConnectCallback = std::function<void(TlsClient&, const Error&)>;
    using CloseCallback = std::function<void(TlsClient&, const Error&)>;
    using EndSendCallback = std::function<void(TlsClient&, const Error&)>;
    using DataReceiveCallback = std::function<void(TlsClient&, const DataChunk&, const Error&)>;

    TARM_IO_FORBID_COPY(TlsClient);
    TARM_IO_FORBID_MOVE(TlsClient);

    TARM_IO_DLL_PUBLIC void schedule_removal() override;

    TARM_IO_DLL_PUBLIC const Endpoint& endpoint() const;

    TARM_IO_DLL_PUBLIC
    void connect(const Endpoint endpoint,
                 const ConnectCallback& connect_callback,
                 const DataReceiveCallback& receive_callback = nullptr,
                 const CloseCallback& close_callback = nullptr);
    TARM_IO_DLL_PUBLIC void close();

    TARM_IO_DLL_PUBLIC bool is_open() const;

    // TODO: test this 'copy_and_send_data'
    TARM_IO_DLL_PUBLIC void copy_and_send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr);
    // TODO: unique ptr send + test
    TARM_IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(const std::string& message, const EndSendCallback& callback = nullptr);
    TARM_IO_DLL_PUBLIC void send_data(std::string&& message, const EndSendCallback& callback = nullptr);

    TARM_IO_DLL_PUBLIC TlsVersion negotiated_tls_version() const;

protected:
    TARM_IO_DLL_PUBLIC TlsClient(AllocationContext& context);
    TARM_IO_DLL_PUBLIC TlsClient(AllocationContext& context, TlsVersionRange version_range);

    TARM_IO_DLL_PUBLIC ~TlsClient();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace net
} // namespace io
} // namespace tarm

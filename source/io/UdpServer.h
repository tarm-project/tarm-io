#pragma once

#include "EventLoop.h"
#include "Export.h"
#include "DataChunk.h"
#include "Disposable.h"
#include "Error.h"
#include "UserDataHolder.h"
#include "UdpPeer.h"

#include <memory>

namespace io {

class UdpServer : public Disposable,
                  public UserDataHolder {
public:
    using DataReceivedCallback = std::function<void(UdpServer&, UdpPeer&, const DataChunk&, const Error&)>;

    // TODO: the type is unused
    using EndSendCallback = std::function<void(UdpServer&, const Error&)>;

    UdpServer(const UdpServer& other) = delete;
    UdpServer& operator=(const UdpServer& other) = delete;

    UdpServer(UdpServer&& other) = default;
    UdpServer& operator=(UdpServer&& other) = default;

    IO_DLL_PUBLIC UdpServer(EventLoop& loop);

    IO_DLL_PUBLIC void schedule_removal() override;

    IO_DLL_PUBLIC Error bind(const std::string& ip_addr_str, std::uint16_t port);

    IO_DLL_PUBLIC void start_receive(DataReceivedCallback receive_callback);

    IO_DLL_PUBLIC void close();

protected:
    IO_DLL_PUBLIC ~UdpServer();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

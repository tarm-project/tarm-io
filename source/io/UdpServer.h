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
    using NewPeerCallback = std::function<void(UdpServer&, UdpPeer&, const Error&)>;
    using DataReceivedCallback = std::function<void(UdpServer&, UdpPeer&, const DataChunk&, const Error&)>;
    using PeerTimeoutCallback = std::function<void(UdpServer&, UdpPeer&)>;

    UdpServer(const UdpServer& other) = delete;
    UdpServer& operator=(const UdpServer& other) = delete;

    UdpServer(UdpServer&& other) = default;
    UdpServer& operator=(UdpServer&& other) = default;

    IO_DLL_PUBLIC UdpServer(EventLoop& loop);

    IO_DLL_PUBLIC void schedule_removal() override;

    IO_DLL_PUBLIC Error start_receive(const std::string& ip_addr_str, std::uint16_t port, DataReceivedCallback receive_callback);
    IO_DLL_PUBLIC Error start_receive(const std::string& ip_addr_str, std::uint16_t port, NewPeerCallback new_peer_callback, DataReceivedCallback receive_callback, std::size_t timeout_ms, PeerTimeoutCallback timeout_callback);
    IO_DLL_PUBLIC Error start_receive(const std::string& ip_addr_str, std::uint16_t port, DataReceivedCallback receive_callback, std::size_t timeout_ms, PeerTimeoutCallback timeout_callback);

    IO_DLL_PUBLIC void close();

protected:
    IO_DLL_PUBLIC ~UdpServer();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

#pragma once

#include "BufferSizeResult.h"
#include "CommonMacros.h"
#include "Endpoint.h"
#include "EventLoop.h"
#include "Export.h"
#include "Forward.h"
#include "DataChunk.h"
#include "Removable.h"
#include "Error.h"
#include "UserDataHolder.h"
#include "UdpPeer.h"

#include <memory>

namespace io {

class UdpServer : public Removable,
                  public UserDataHolder {
public:
    using NewPeerCallback = std::function<void(UdpPeer&, const Error&)>;
    using DataReceivedCallback = std::function<void(UdpPeer&, const DataChunk&, const Error&)>;
    using PeerTimeoutCallback = std::function<void(UdpPeer&, const Error&)>;

    IO_FORBID_COPY(UdpServer);
    IO_FORBID_MOVE(UdpServer);

    IO_DLL_PUBLIC UdpServer(EventLoop& loop);

    IO_DLL_PUBLIC void schedule_removal() override;

    IO_DLL_PUBLIC Error start_receive(const Endpoint& endpoint,
                                      DataReceivedCallback receive_callback);

    IO_DLL_PUBLIC Error start_receive(const Endpoint& endpoint,
                                      NewPeerCallback new_peer_callback,
                                      DataReceivedCallback receive_callback,
                                      std::size_t timeout_ms,
                                      PeerTimeoutCallback timeout_callback);

    IO_DLL_PUBLIC Error start_receive(const Endpoint& endpoint,
                                      DataReceivedCallback receive_callback,
                                      std::size_t timeout_ms,
                                      PeerTimeoutCallback timeout_callback);

    IO_DLL_PUBLIC void close();

    IO_DLL_PUBLIC BufferSizeResult receive_buffer_size() const;
    IO_DLL_PUBLIC BufferSizeResult send_buffer_size() const;
    IO_DLL_PUBLIC Error set_receive_buffer_size(std::size_t size);
    IO_DLL_PUBLIC Error set_send_buffer_size(std::size_t size);

protected:
    IO_DLL_PUBLIC ~UdpServer();

private:
    friend class UdpPeer;

    void close_peer(UdpPeer& peer, std::size_t inactivity_timeout_ms);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

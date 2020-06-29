/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "../CommonMacros.h"
#include "../EventLoop.h"
#include "../Export.h"
#include "../Forward.h"
#include "../DataChunk.h"
#include "../Removable.h"
#include "../Error.h"
#include "../UserDataHolder.h"

#include "BufferSizeResult.h"

#include "Endpoint.h"
#include "UdpPeer.h"

#include <memory>

namespace tarm {
namespace io {
namespace net {

class UdpServer : public Removable,
                  public UserDataHolder {
public:
    using NewPeerCallback = std::function<void(UdpPeer&, const Error&)>;
    using DataReceivedCallback = std::function<void(UdpPeer&, const DataChunk&, const Error&)>;
    using PeerTimeoutCallback = std::function<void(UdpPeer&, const Error&)>;

    using CloseServerCallback = std::function<void(UdpServer&, const Error&)>;

    IO_FORBID_COPY(UdpServer);
    IO_FORBID_MOVE(UdpServer);

    IO_DLL_PUBLIC UdpServer(EventLoop& loop);

    IO_DLL_PUBLIC void schedule_removal() override;

    IO_DLL_PUBLIC Error start_receive(const Endpoint& endpoint,
                                      const DataReceivedCallback& receive_callback);

    IO_DLL_PUBLIC Error start_receive(const Endpoint& endpoint,
                                      const NewPeerCallback& new_peer_callback,
                                      const DataReceivedCallback& receive_callback,
                                      std::size_t timeout_ms,
                                      const PeerTimeoutCallback& timeout_callback);

    IO_DLL_PUBLIC Error start_receive(const Endpoint& endpoint,
                                      const DataReceivedCallback& receive_callback,
                                      std::size_t timeout_ms,
                                      const PeerTimeoutCallback& timeout_callback);

    IO_DLL_PUBLIC void close(const CloseServerCallback& close_callback = nullptr);

    IO_DLL_PUBLIC BufferSizeResult receive_buffer_size() const;
    IO_DLL_PUBLIC BufferSizeResult send_buffer_size() const;
    IO_DLL_PUBLIC Error set_receive_buffer_size(std::size_t size);
    IO_DLL_PUBLIC Error set_send_buffer_size(std::size_t size);

    // TODO: peers count???
    // TODO: method to iterate on peers???

protected:
    IO_DLL_PUBLIC ~UdpServer();

private:
    friend class UdpPeer;

    void close_peer(UdpPeer& peer, std::size_t inactivity_timeout_ms);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace net
} // namespace io
} // namespace tarm

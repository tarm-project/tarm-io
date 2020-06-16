/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "Endpoint.h"
#include "Error.h"
#include "EventLoop.h"
#include "Export.h"
#include "Forward.h"
#include "UserDataHolder.h"
#include "RefCounted.h"
#include "Removable.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace tarm {
namespace io {

class UdpPeer : public UserDataHolder,
                protected RefCounted {
public:
    friend class UdpServer;

    using EndSendCallback = std::function<void(UdpPeer&, const Error&)>;

    IO_FORBID_COPY(UdpPeer);
    IO_FORBID_MOVE(UdpPeer);

    IO_DLL_PUBLIC void send_data(const char* c_str, std::uint32_t size, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(const std::string& message, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(std::string&& message, EndSendCallback callback = nullptr);

    IO_DLL_PUBLIC const Endpoint& endpoint() const;

    IO_DLL_PUBLIC std::uint64_t last_packet_time() const;

    IO_DLL_PUBLIC bool is_open() const;

    using Removable::set_on_schedule_removal;

    IO_DLL_PUBLIC UdpServer& server();
    IO_DLL_PUBLIC const UdpServer& server() const;

    // DOC: as UDP is connectionless protocol and it may take a while when other side acknowledge end of connection.
    //      Using inactivity timeout to ignore all packets from that peer during a specified period of time.
    //      After that if other side still continues to send packets it will be treated as a new peer.
    // TODO: what about peers with no bookkeeping enabled? Do we need to enable bookkeeping by default???
    //       or enable it for peers which were closed only?
    IO_DLL_PUBLIC void close(std::size_t inactivity_timeout_ms = 1000);

protected:
    IO_DLL_PUBLIC ~UdpPeer();

private:
    IO_DLL_PUBLIC UdpPeer(EventLoop& loop, UdpServer& server, void* udp_handle, const Endpoint& endpoint, const detail::PeerId& id);
    IO_DLL_PUBLIC void set_last_packet_time(std::uint64_t time);
    const detail::PeerId& id() const;

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
} // namespace tarm

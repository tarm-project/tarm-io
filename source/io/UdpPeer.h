#pragma once


//#include "Export.h"
//#include "DataChunk.h"
#include "Disposable.h"

#include "Error.h"
#include "EventLoop.h"
#include "Forward.h"
#include "UserDataHolder.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace io {

class UdpPeer : public UserDataHolder,
                protected Disposable,
                protected RefCounted {
public:
    friend class UdpServer;

    using EndSendCallback = std::function<void(UdpPeer&, const Error&)>;

    IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(const std::string& message, EndSendCallback callback = nullptr);

    IO_DLL_PUBLIC std::uint32_t address() const;
    IO_DLL_PUBLIC std::uint16_t port() const;

    IO_DLL_PUBLIC std::uint64_t last_packet_time_ns() const;

    // TODO: make protected
    IO_DLL_PUBLIC ~UdpPeer();

private:
    IO_DLL_PUBLIC UdpPeer(EventLoop& loop, void* udp_handle, std::uint32_t address, std::uint16_t port);

    IO_DLL_PUBLIC void set_last_packet_time_ns(std::uint64_t time);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
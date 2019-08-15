#pragma once

#include "EventLoop.h"
#include "Export.h"
#include "DataChunk.h"
#include "Disposable.h"
#include "Status.h"
#include "UserDataHolder.h"

#include <memory>

namespace io {

// TODO: fwd include
class UdpClient;

class UdpServer : public Disposable,
                  public UserDataHolder {
public:
    using DataReceivedCallback = std::function<void(UdpServer&, std::uint32_t, std::uint16_t, const DataChunk&, const Status&)>;

UdpServer(const UdpServer& other) = delete;
    UdpServer& operator=(const UdpServer& other) = delete;

    UdpServer(UdpServer&& other) = default;
    UdpServer& operator=(UdpServer&& other) = default;

    IO_DLL_PUBLIC UdpServer(EventLoop& loop);

    IO_DLL_PUBLIC void schedule_removal() override;

    IO_DLL_PUBLIC Status bind(const std::string& ip_addr_str, std::uint16_t port);

    IO_DLL_PUBLIC void start_receive(DataReceivedCallback receive_callback);

    IO_DLL_PUBLIC void close();

protected:
    IO_DLL_PUBLIC ~UdpServer();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
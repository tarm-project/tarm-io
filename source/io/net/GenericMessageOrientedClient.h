/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "Error.h"
#include "DataChunk.h"
#include "Removable.h"
#include "net/Endpoint.h"
// TODO: as this template class has to be public, it should not  depend on private detail class
#include "../detail/VariableLengthSize.h"

#include <cstddef>
#include <functional>
#include <memory>

namespace tarm {
namespace io {
namespace net {

template<typename ClientType>
class GenericMessageOrientedClient {
public:
    using ClientPtr = std::unique_ptr<ClientType, typename Removable::DefaultDelete>;

    using ConnectCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const Error&)>;
    using DataReceiveCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const Error&)>;
    using EndSendCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const Error&)>;

    static constexpr std::size_t DEFAULT_MAX_SIZE = 2 * 1024 * 1024; // 2MB

    GenericMessageOrientedClient(ClientPtr client, std::size_t max_message_size = DEFAULT_MAX_SIZE) :
        m_max_message_size(max_message_size),
        m_client(std::move(client)) {
        m_buffer.reset(new char[max_message_size]);
    }

    void connect(const Endpoint& endpoint,
                 const ConnectCallback& connect_callback,
                 const DataReceiveCallback& receive_callback = nullptr,
                 const CloseCallback& close_callback = nullptr) {
        m_client->connect(
            endpoint,
            [=](ClientType&, const Error& error) {
                if (connect_callback) {
                    connect_callback(*this, error);
                }
            },
            [=](ClientType&, const DataChunk&, const Error&) {

            },
            [=](ClientType&, const Error&) {

            }
        );
    }

    void send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr) {
        ::tarm::io::detail::VariableLengthSize vs(size);
        if (vs.is_complete() && vs.bytes_count()) {
            m_client->send_data(reinterpret_cast<const char*>(vs.bytes()), vs.bytes_count());
            m_client->send_data(c_str, size);
        } else {
            // TODO: error
        }
    }

    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback = nullptr);
    void send_data(const std::string& message, const EndSendCallback& callback = nullptr);
    void send_data(std::string&& message, const EndSendCallback& callback = nullptr);

    ClientType& client() {
        return *m_client;
    }

    const ClientType& client() const {
        return *m_client;
    }

private:
    std::size_t m_max_message_size;

    ClientPtr m_client;
    std::unique_ptr<char, std::default_delete<char[]>> m_buffer;
};

} // namespace net
} // namespace io
} // namespace tarm

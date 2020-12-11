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
    // Messages larger than this size are skipped with reporting of an appropriate error
    static constexpr std::size_t DEFAULT_MAX_SIZE = 2 * 1024 * 1024; // 2MB

    using ClientPtr = std::unique_ptr<ClientType, typename Removable::DefaultDelete>;

    using ConnectCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const Error&)>;
    using DataReceiveCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const Error&)>;
    using EndSendCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const Error&)>;

    GenericMessageOrientedClient(ClientPtr client, std::size_t max_message_size = DEFAULT_MAX_SIZE) :
        m_max_message_size(max_message_size),
        m_client(std::move(client)) {
        m_buffer.reset(new char[max_message_size], std::default_delete<char[]>());
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
            [=](ClientType&, const DataChunk& data, const Error& error) {
                if (error) {
                    // TODO: pass error to receive_callback
                    return;
                }

                std::size_t bytes_left = data.size;
                std::size_t bytes_processed = data.size - bytes_left;
                while (bytes_left) {
                    if (!m_current_message_size.is_complete()) {
                        bytes_processed = data.size - bytes_left;
                        const auto size_bytes = m_current_message_size.add_bytes(
                            reinterpret_cast<const std::uint8_t*>(data.buf.get() + bytes_processed), bytes_left);
                        if (size_bytes > bytes_left) {
                            // TODO: error handling here
                            // This could happen in case of corrupted stream
                            return;
                        }

                        if (current_message_too_large()) {
                            if (receive_callback) {
                                receive_callback(*this, {nullptr, m_current_message_size.value()}, StatusCode::MESSAGE_TOO_LONG);
                            }
                        }

                        bytes_left -= size_bytes;
                        if (bytes_left == 0) {
                            return;
                        }
                    }

                    bytes_processed = data.size - bytes_left;

                    const std::size_t size_to_copy = (m_current_message_size.value() - m_current_message_offset) < bytes_left ?
                                                     (m_current_message_size.value() - m_current_message_offset) :
                                                     bytes_left;

                    if (!current_message_too_large()) {
                        std::memcpy(m_buffer.get() + m_current_message_offset, data.buf.get() + bytes_processed, size_to_copy);
                    }
                    m_current_message_offset += size_to_copy;
                    bytes_left -= size_to_copy;

                    if (m_current_message_offset == m_current_message_size.value()) {
                        if (receive_callback && !current_message_too_large())  {
                            receive_callback(*this, {m_buffer, m_current_message_size.value()}, error);
                            // TODO: chaeck use count of that shared ptr and allocate new buffer if required
                        }
                        m_current_message_size.reset();
                        m_current_message_offset = 0;
                    }
                }
            },
            [=](ClientType&, const Error& error) {
                if (close_callback) {
                    close_callback(*this, error);
                }

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
    bool current_message_too_large() const {
        return m_current_message_size.is_complete() && m_current_message_size.value() > m_max_message_size;
    }

    std::size_t m_max_message_size;
    std::size_t m_current_message_offset = 0;
    ::tarm::io::detail::VariableLengthSize m_current_message_size;

    ClientPtr m_client;
    std::shared_ptr<char> m_buffer;
};

} // namespace net
} // namespace io
} // namespace tarm

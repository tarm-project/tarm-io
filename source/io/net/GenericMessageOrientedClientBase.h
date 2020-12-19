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

// Messages larger than this size are skipped with reporting of an appropriate error
// TODO: rename
static constexpr std::size_t BLA_BLA_DEFAULT_MAX_SIZE = 2 * 1024 * 1024; // 2MB

template<typename ClientType, typename ParentType>
class GenericMessageOrientedClientBase {
public:
    GenericMessageOrientedClientBase(ClientType* client, std::size_t max_message_size) :
        m_max_message_size(max_message_size),
        m_client(client) {
        m_buffer.reset(new char[max_message_size], std::default_delete<char[]>());
    }

    template<typename ReceiveCallback>
    void on_data_receive(ReceiveCallback& receive_callback, const DataChunk& data, const Error& error) {
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
                        receive_callback(static_cast<ParentType&>(*this), {nullptr, static_cast<std::size_t>(m_current_message_size.value())}, StatusCode::MESSAGE_TOO_LONG);
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
                    receive_callback(static_cast<ParentType&>(*this), {m_buffer, static_cast<std::size_t>(m_current_message_size.value())}, error);
                    // TODO: chaeck use count of that shared ptr and allocate new buffer if required
                }
                m_current_message_size.reset();
                m_current_message_offset = 0;
            }
        }
    }

    ClientType& client() {
        return *m_client;
    }

    const ClientType& client() const {
        return *m_client;
    }

protected:
    template<typename EndSendCallback>
    void send_data_impl(const char* c_str, std::uint32_t size, const EndSendCallback& callback) {
        this->do_send_size(size); // TODO: size send error handling?
        if (callback) {
            this->m_client->send_data(c_str, size,
                [=](ClientType&, const Error& error) {
                    callback(static_cast<ParentType&>(*this), error);
                }
            );
        } else {
            this->m_client->send_data(c_str, size);
        }
    }

    template<typename EndSendCallback>
    void send_data_impl(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback) {
        this->do_send_size(size); // TODO: size send error handling?
        if (callback) {
            this->m_client->send_data(buffer, size,
                [=](ClientType&, const Error& error) {
                    callback(static_cast<ParentType&>(*this), error);
                }
            );
        } else {
            this->m_client->send_data(buffer, size);
        }
    }

    template<typename EndSendCallback>
    void send_data_impl(const std::string& message, const EndSendCallback& callback) {
        this->do_send_size(message.size()); // TODO: size send error handling?
        if (callback) {
            this->m_client->send_data(message,
                [=](ClientType&, const Error& error) {
                    callback(static_cast<ParentType&>(*this), error);
                }
            );
        } else {
            this->m_client->send_data(message);
        }
    }

    template<typename EndSendCallback>
    void send_data_impl(std::string&& message, const EndSendCallback& callback) {
        this->do_send_size(message.size()); // TODO: size send error handling?
        if (callback) {
            this->m_client->send_data(std::move(message),
                [=](ClientType&, const Error& error) {
                    callback(static_cast<ParentType&>(*this), error);
                }
            );
        } else {
            this->m_client->send_data(std::move(message));
        }
    }

    bool current_message_too_large() const {
        return m_current_message_size.is_complete() && m_current_message_size.value() > m_max_message_size;
    }

    template<typename SizeType>
    void do_send_size(SizeType size) {
        // TODO: handle zero size
        ::tarm::io::detail::VariableLengthSize vs(size);
        // TODO: handle send error
        m_client->copy_and_send_data(reinterpret_cast<const char*>(vs.bytes()), vs.bytes_count());
    }

    std::size_t m_max_message_size;
    std::size_t m_current_message_offset = 0;
    ::tarm::io::detail::VariableLengthSize m_current_message_size;

    std::shared_ptr<char> m_buffer;
    ClientType* m_client;
};

} // namespace net
} // namespace io
} // namespace tarm

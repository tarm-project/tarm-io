/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "Error.h"
#include "DataChunk.h"
#include "Removable.h"
#include "net/Endpoint.h"
#include "core/VariableLengthSize.h"

#include <assert.h>
#include <cstddef>
#include <functional>
#include <memory>

namespace tarm {
namespace io {
namespace net {

template<typename ClientType, typename ParentType>
class GenericMessageOrientedClientBase {
public:
    // Max message size for send and receive
    static constexpr std::size_t DEFAULT_MAX_SIZE = 2 * 1024 * 1024; // 2MB

    GenericMessageOrientedClientBase(ClientType* client, std::size_t max_message_size) :
        m_max_message_size(max_message_size),
        m_client(client) {
        m_buffer.reset(new char[max_message_size], std::default_delete<char[]>());
    }

    template<typename ReceiveCallback>
    void on_data_receive(ReceiveCallback& receive_callback, const DataChunk& data, const Error& error) {
        if (error) {
            if (receive_callback) {
                receive_callback(static_cast<ParentType&>(*this), {nullptr, 0}, error);
            }
            return;
        }

        std::size_t bytes_left = data.size;
        std::size_t bytes_processed = data.size - bytes_left;
        while (bytes_left) {
            if (!m_current_message_size.is_complete()) {
                bytes_processed = data.size - bytes_left;
                const auto size_bytes = m_current_message_size.add_bytes(
                    reinterpret_cast<const std::uint8_t*>(data.buf.get() + bytes_processed), bytes_left);
                assert(size_bytes <= bytes_left);

                if (current_message_too_large()) {
                    if (receive_callback) {
                        receive_callback(static_cast<ParentType&>(*this), {nullptr, static_cast<std::size_t>(m_current_message_size.value())}, StatusCode::MESSAGE_TOO_LONG);
                    }
                }

                if (m_current_message_size.fail()) {
                    if (receive_callback) {
                        receive_callback(static_cast<ParentType&>(*this), {nullptr, 0}, StatusCode::PROTOCOL_ERROR);
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
    bool send_size_impl(const EndSendCallback& callback, const char* /*c_str*/, std::uint32_t size) {
        return do_send_size(callback, size);
    }

    template<typename EndSendCallback>
    bool send_size_impl(const EndSendCallback& callback, const std::shared_ptr<const char>& /*buffer*/, std::uint32_t size) {
        return do_send_size(callback, size);
    }

    template<typename EndSendCallback>
    bool send_size_impl(const EndSendCallback& callback, const std::unique_ptr<char[]>& /*buffer*/, std::uint32_t size) {
        return do_send_size(callback, size);
    }

    template<typename EndSendCallback>
    bool send_size_impl(const EndSendCallback& callback, const std::string& message) {
        return do_send_size(callback, message.size());
    }

    template<typename EndSendCallback, typename... Params>
    void send_data_impl(const EndSendCallback& callback, Params&&... params) {
        const bool resume = this->send_size_impl(callback, std::forward<Params>(params)...);
        if (!resume) {
            return;
        }

        if (callback) {
            this->m_client->send_data(std::forward<Params>(params)...,
                [=](ClientType&, const Error& error) {
                    callback(static_cast<ParentType&>(*this), error);
                }
            );
        } else {
            this->m_client->send_data(std::forward<Params>(params)...);
        }
    }

    bool current_message_too_large() const {
        return m_current_message_size.is_complete() && m_current_message_size.value() > m_max_message_size;
    }

    template<typename SizeType, typename EndSendCallback>
    bool do_send_size(const EndSendCallback& callback, SizeType size) {
        if (size == 0) {
            if (callback) {
                callback(static_cast<ParentType&>(*this), StatusCode::INVALID_ARGUMENT);
            }
            return false;
        }

        if (size > m_max_message_size) {
            if (callback) {
                callback(static_cast<ParentType&>(*this), StatusCode::MESSAGE_TOO_LONG);
            }
            return false;
        }

        core::VariableLengthSize vs(size);
        // TODO: handle send error
        m_client->copy_and_send_data(reinterpret_cast<const char*>(vs.bytes()), vs.bytes_count());
        return true;
    }

    std::size_t m_max_message_size;
    std::size_t m_current_message_offset = 0;
    core::VariableLengthSize m_current_message_size;

    std::shared_ptr<char> m_buffer;
    ClientType* m_client;
};

} // namespace net
} // namespace io
} // namespace tarm

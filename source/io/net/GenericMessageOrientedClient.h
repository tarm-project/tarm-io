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

template<typename ClientType>
class GenericMessageOrientedClient : public GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedClient<ClientType>> {
public:
    static constexpr std::size_t DEFAULT_MAX_SIZE = BLA_BLA_DEFAULT_MAX_SIZE;

    using ConnectCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const Error&)>;
    using DataReceiveCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const Error&)>;
    using EndSendCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const Error&)>;

    using ClientPtr = std::unique_ptr<ClientType, typename Removable::DefaultDelete>;

    GenericMessageOrientedClient(ClientPtr client, std::size_t max_message_size = DEFAULT_MAX_SIZE) :
        GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedClient<ClientType>>(client.get(),  max_message_size),
        m_client_ptr(std::move(client)) {
    }

    //using GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedClient<ClientType>>::GenericMessageOrientedClientBase;

    void connect(const Endpoint& endpoint,
                 const ConnectCallback& connect_callback,
                 const DataReceiveCallback& receive_callback = nullptr,
                 const CloseCallback& close_callback = nullptr) {
        this->m_client->connect(
            endpoint,
            [=](ClientType&, const Error& error) {
                if (connect_callback) {
                    connect_callback(*this, error);
                }
            },
            [=](ClientType&, const DataChunk& data, const Error& error) {
                this->on_data_receive(receive_callback, data, error);
            },
            [=](ClientType&, const Error& error) {
                if (close_callback) {
                    close_callback(*this, error);
                }

            }
        );
    }

    void send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr) {
        this->do_send_size(size);
        this->m_client->send_data(c_str, size);
    }

    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback = nullptr);
    void send_data(const std::string& message, const EndSendCallback& callback = nullptr);
    void send_data(std::string&& message, const EndSendCallback& callback = nullptr);

private:
    ClientPtr m_client_ptr;
};

template<typename ClientType>
class GenericMessageOrientedConnectedClient : public GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedConnectedClient<ClientType>> {
public:
    static constexpr std::size_t DEFAULT_MAX_SIZE = BLA_BLA_DEFAULT_MAX_SIZE;

    GenericMessageOrientedConnectedClient(ClientType* client) :
        GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedConnectedClient<ClientType>>(client, DEFAULT_MAX_SIZE) { // TODO: inherit max message size from server????
    }
};

template<typename ServerType>
class GenericMessageOrientedServer {
public:
    using ServerPtr = std::unique_ptr<ServerType, typename Removable::DefaultDelete>;

    using NewConnectionCallback = std::function<void(GenericMessageOrientedConnectedClient<typename ServerType::ConnectedClientType>&, const Error&)>;
    using DataReceivedCallback = std::function<void(GenericMessageOrientedConnectedClient<typename ServerType::ConnectedClientType>&, const DataChunk&, const Error&)>;
    using CloseConnectionCallback = std::function<void(GenericMessageOrientedConnectedClient<typename ServerType::ConnectedClientType>&, const Error&)>;

    GenericMessageOrientedServer(ServerPtr server) :
        m_server(std::move(server)) {

    }

    ServerType& server() {
        return *m_server;
    }

    const ServerType& server() const {
        return *m_server;
    }

    Error listen(const Endpoint& endpoint,
                 const NewConnectionCallback& new_connection_callback,
                 const DataReceivedCallback& data_receive_callback,
                 const CloseConnectionCallback& close_connection_callback) {
        return m_server->listen(
            endpoint,
            [=](typename ServerType::ConnectedClientType& client, const io::Error& error) {
                auto connected_client_wrapper = new GenericMessageOrientedConnectedClient<typename ServerType::ConnectedClientType>(&client);
                if (new_connection_callback) {
                    new_connection_callback(*connected_client_wrapper, error);
                }
                client.set_user_data(connected_client_wrapper);
            },
            [=](typename ServerType::ConnectedClientType& client, const io::DataChunk& data, const io::Error& error) {
                auto connected_client_wrapper = client.template user_data_as_ptr<GenericMessageOrientedConnectedClient<typename ServerType::ConnectedClientType>>();
                connected_client_wrapper->on_data_receive(data_receive_callback, data, error);
            },
            [=](typename ServerType::ConnectedClientType& client, const io::Error& error) {
                auto connected_client_wrapper = client.template user_data_as_ptr<GenericMessageOrientedConnectedClient<typename ServerType::ConnectedClientType>>();
                if (close_connection_callback) {
                    close_connection_callback(*connected_client_wrapper, error);
                }
                client.set_user_data(nullptr);
                delete connected_client_wrapper;
            }
        );
    }

private:
    ServerPtr m_server;
};

} // namespace net
} // namespace io
} // namespace tarm

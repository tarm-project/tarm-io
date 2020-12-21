/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "GenericMessageOrientedClientBase.h"

namespace tarm {
namespace io {
namespace net {

template<typename ClientType>
class GenericMessageOrientedClient : public GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedClient<ClientType>> {
public:
    static constexpr std::size_t DEFAULT_MAX_MESSAGE_SIZE = GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedClient<ClientType>>::DEFAULT_MAX_SIZE;

    using ConnectCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const Error&)>;
    using DataReceiveCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const Error&)>;
    using EndSendCallback = std::function<void(GenericMessageOrientedClient<ClientType>&, const Error&)>;

    using ClientPtr = std::unique_ptr<ClientType, typename Removable::DefaultDelete>;

    GenericMessageOrientedClient(ClientPtr client, std::size_t max_message_size = DEFAULT_MAX_MESSAGE_SIZE) :
        GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedClient<ClientType>>(client.get(),  max_message_size),
        m_client_ptr(std::move(client)) {
    }

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

    // TODO: unique_ptr
    void send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr) {
        GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedClient<ClientType>>::send_data_impl(callback, c_str, size);
    }

    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback = nullptr) {
        GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedClient<ClientType>>::send_data_impl(callback, buffer, size);
    }

    void send_data(const std::string& message, const EndSendCallback& callback = nullptr) {
        GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedClient<ClientType>>::send_data_impl(callback, message);
    }

    void send_data(std::string&& message, const EndSendCallback& callback = nullptr) {
        GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedClient<ClientType>>::send_data_impl(callback, std::move(message));
    }

private:
    ClientPtr m_client_ptr;
};

} // namespace net
} // namespace io
} // namespace tarm

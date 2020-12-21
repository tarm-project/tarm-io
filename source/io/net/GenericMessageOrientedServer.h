/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "GenericMessageOrientedConnectedClient.h"

namespace tarm {
namespace io {
namespace net {

template<typename ServerType>
class GenericMessageOrientedServer {
public:
    static constexpr std::size_t DEFAULT_MAX_MESSAGE_SIZE = GenericMessageOrientedClientBase<typename ServerType::ConnectedClientType, GenericMessageOrientedClient<typename ServerType::ConnectedClientType>>::DEFAULT_MAX_SIZE;

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
                auto connected_client_wrapper = new GenericMessageOrientedConnectedClient<typename ServerType::ConnectedClientType>(&client, DEFAULT_MAX_MESSAGE_SIZE); // TODO: variable instead of constant
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

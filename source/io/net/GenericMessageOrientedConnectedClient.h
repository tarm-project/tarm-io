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
class GenericMessageOrientedConnectedClient : public GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedConnectedClient<ClientType>> {
public:
    using EndSendCallback = std::function<void(GenericMessageOrientedConnectedClient<ClientType>&, const Error&)>;

    GenericMessageOrientedConnectedClient(ClientType* client, std::size_t max_message_size) :
        GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedConnectedClient<ClientType>>(client, max_message_size) {
    }

    void send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr) {
        this->send_data_impl(callback, c_str, size);
    }

    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback = nullptr) {
        this->send_data_impl(size, callback, buffer);
    }

    void send_data(std::unique_ptr<char[]> buffer, std::uint32_t size, const EndSendCallback& callback = nullptr) {
        this->send_data_impl(callback, std::move(buffer), size);
    }

    void send_data(const std::string& message, const EndSendCallback& callback = nullptr) {
        this->send_data_impl(callback, message);
    }

    void send_data(std::string&& message, const EndSendCallback& callback = nullptr) {
        this->send_data_impl(callback, std::move(message));
    }
};

} // namespace net
} // namespace io
} // namespace tarm

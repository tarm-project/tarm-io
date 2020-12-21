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
    static constexpr std::size_t DEFAULT_MAX_SIZE = BLA_BLA_DEFAULT_MAX_SIZE;

    using EndSendCallback = std::function<void(GenericMessageOrientedConnectedClient<ClientType>&, const Error&)>;

    GenericMessageOrientedConnectedClient(ClientType* client) :
        GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedConnectedClient<ClientType>>(client, DEFAULT_MAX_SIZE) { // TODO: inherit max message size from server????
    }

    // TODO: unique_ptr
    void send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback = nullptr) {
        this->send_data_impl(callback, c_str, size);
    }

    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback = nullptr) {
        this->send_data_impl(size, callback, buffer);
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

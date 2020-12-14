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

    GenericMessageOrientedConnectedClient(ClientType* client) :
        GenericMessageOrientedClientBase<ClientType, GenericMessageOrientedConnectedClient<ClientType>>(client, DEFAULT_MAX_SIZE) { // TODO: inherit max message size from server????
    }
};

} // namespace net
} // namespace io
} // namespace tarm

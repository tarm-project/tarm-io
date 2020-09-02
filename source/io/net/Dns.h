/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "../Error.h"
#include "../EventLoop.h"
#include "../Export.h"

#include "Endpoint.h"

#include <functional>
#include <string>
#include <vector>

namespace tarm {
namespace io {
namespace net {

using ResolveCallback = std::function<void(const std::vector<Endpoint>&, const Error&)>;
TARM_IO_DLL_PUBLIC void resolve_host(EventLoop& loop,
                                     const std::string& host_name,
                                     const ResolveCallback& callback,
                                     Endpoint::Type protocol = Endpoint::Type::ANY);

} // namespace net
} // namespace io
} // namespace tarm

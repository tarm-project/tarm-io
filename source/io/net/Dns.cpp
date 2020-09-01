/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "Dns.h"

#include "../detail/EventLoopHelpers.h"

#include <uv.h>

namespace tarm {
namespace io {
namespace net {

struct GetaddrInfoRequest : public uv_getaddrinfo_t {
    ResolveCallback callback;
};

void on_getaddrinfo(uv_getaddrinfo_t* req, int status, addrinfo* res) {
    auto& request = *reinterpret_cast<GetaddrInfoRequest*>(req);

    std::vector<Endpoint> endpoints;
    endpoints.reserve(10);

    auto current = res;
    while (current) {
        endpoints.emplace_back(reinterpret_cast<const Endpoint::sockaddr_placeholder*>(current->ai_addr));
        current = current->ai_next;
    }

    request.callback(endpoints, Error(status));

    uv_freeaddrinfo(res);
    delete &request;
}

void resolve_host_impl(EventLoop& loop, const std::string& host_name, const ResolveCallback& callback, Endpoint::Type protocol) {
    auto request = new GetaddrInfoRequest;
    request->callback = callback;
    addrinfo hints;
    hints.ai_family = raw_type(protocol);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;//AI_V4MAPPED | AI_ADDRCONFIG; // TODO: test this
    hints.ai_protocol = 0;
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    Error error = uv_getaddrinfo(reinterpret_cast<uv_loop_t*>(loop.raw_loop()),
                                 request,
                                 on_getaddrinfo,
                                 host_name.c_str(),
                                 /*const char* service*/nullptr,
                                 &hints);
    if (error) {
        if (callback) {
            callback(std::vector<Endpoint>(), error);
        }
        delete request;
    }
}

void resolve_host(EventLoop& loop, const std::string& host_name, const ResolveCallback& callback, Endpoint::Type protocol) {
    ::tarm::io::detail::defer_execution_if_required(loop, resolve_host_impl, loop, host_name, callback, protocol);
}

} // namespace net
} // namespace io
} // namespace tarm
/*----------------------------------------------------------------------------------------------
*  Copyright (c) 2020 - present Alexander Voitenko
*  Licensed under the MIT License. See License.txt in the project root for license information.
*----------------------------------------------------------------------------------------------*/

#include "Functions.h"

#include <uv.h>

#include <utility>

namespace tarm {
namespace io {
namespace fs {

// TODO: move to some common location and reuse
template<typename F, typename... Params>
void defer_execution_if_required(EventLoop& loop, const F& f, Params&&... params) {
    if (loop.is_running()) {
        f(loop, std::forward<Params>(params)...);
    } else {
        loop.execute_on_loop_thread(std::bind(std::forward<F>(f),
                                              std::placeholders::_1,
                                              std::forward<Params>(params)...));
    }
}

struct StatRequest : public uv_fs_t {
    StatCallback callback;
};

void on_stat(uv_fs_t* req) {
    auto& request = *reinterpret_cast<StatRequest*>(req);
    Error error(req->result);
    if (request.callback) {
        request.callback(req->path, *reinterpret_cast<StatData*>(&request.statbuf), error);
    }
    uv_fs_req_cleanup(req);
    delete &request;
}

void stat_impl(EventLoop& loop, const Path& path, const StatCallback& callback) {
    auto request = new StatRequest;
    request->callback = callback;
    const Error stat_error = uv_fs_stat(reinterpret_cast<uv_loop_t*>(loop.raw_loop()),
                                        request,
                                        path.c_str(),
                                        on_stat);
    if (stat_error) {
        StatData data;
        std::memset(&data, 0, sizeof(data));
        if (callback) {
            callback(path, data, stat_error);
        }
        delete request;
        return;
    }
}

void stat(EventLoop& loop, const Path& path, const StatCallback& callback) {
    defer_execution_if_required(loop, stat_impl, path, callback);
}

} // namespace fs
} // namespace io
} // namespace tarm

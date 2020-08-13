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
                                        path.string().c_str(),
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

#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

bool is_regular_file(const StatData& stat_data) {
    return S_ISREG(stat_data.mode);
}

#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
    #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

bool is_directory(const StatData& stat_data) {
    return S_ISDIR(stat_data.mode);
}


//#define S_ISBLK(m)      (((m) & S_IFMT) == S_IFBLK)     /* block special */
//#define S_ISCHR(m)      (((m) & S_IFMT) == S_IFCHR)     /* char special */
//#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)     /* directory */
//#define S_ISFIFO(m)     (((m) & S_IFMT) == S_IFIFO)     /* fifo or socket */
//#define S_ISREG(m)      (((m) & S_IFMT) == S_IFREG)     /* regular file */
//#define S_ISLNK(m)      (((m) & S_IFMT) == S_IFLNK)     /* symbolic link */
//#define S_ISSOCK(m)     (((m) & S_IFMT) == S_IFSOCK)    /* socket */

} // namespace fs
} // namespace io
} // namespace tarm

/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "Dir.h"

#include "detail/Common.h"
#include "detail/EventLoopHelpers.h"
#include "detail/LogMacros.h"
#include "fs/detail/FsCommon.h"
#include "ScopeExitGuard.h"
#include "fs/Functions.h"

#include <cstring>
#include <vector>
#include <assert.h>

namespace tarm {
namespace io {
namespace fs {

struct CloseDirRequest : public uv_fs_t {
    Dir::CloseCallback close_callback;
};

template<typename ListCallback>
struct ReadDirRequest : public uv_fs_t {
    ReadDirRequest(const ListCallback& list,
                   const Dir::EndListCallback& end) :
        list_callback(list),
        end_list_callback(end) {
    }

    ListCallback list_callback;
    Dir::EndListCallback end_list_callback;
};

class Dir::Impl {
public:
    using ParentType = Dir;

    enum class State {
        INITIAL = 0,
        OPENING,
        OPENED,
        WANTCLOSE,
        CLOSING,
        CLOSED
    };

    Impl(AllocationContext& context, Dir& parent);
    ~Impl();

    void open(const Path& path, const OpenCallback& callback);
    bool is_open() const;
    void close(const CloseCallback& callback);

    template<void(*UvCallback)(uv_fs_t*), typename ListCallback>
    void list(const ListCallback& list_callback, const EndListCallback& end_list_callback);

    const Path& path() const;

    bool schedule_removal();

    // statics
    // Unfortunately can not make some solution with class member specialization here because of GCC 4.8.4
    static void on_read_dir_with_continuation(uv_fs_t* req);
    static void on_read_dir_no_continuation(uv_fs_t* req);

    State state() const;
protected:
    // statics
    static void on_open_dir(uv_fs_t* req);
    static void on_close_dir(uv_fs_t* req);

private:
    static constexpr std::size_t DIRENTS_NUMBER = 1;

    Dir* m_parent = nullptr;
    EventLoop* m_loop;
    uv_loop_t* m_uv_loop;

    OpenCallback m_open_callback = nullptr;

    Path m_path;

    uv_fs_t m_open_dir_req;
    uv_fs_t* m_read_request = nullptr;
    uv_dir_t* m_uv_dir = nullptr;

    uv_dirent_t m_dirents[DIRENTS_NUMBER];

    State m_state = State::INITIAL;
};

namespace {

DirectoryEntryType convert_direntry_type(uv_dirent_type_t type) {
    switch (type) {
        case UV_DIRENT_FILE:
            return DirectoryEntryType::FILE;
        case UV_DIRENT_DIR:
            return DirectoryEntryType::DIR;
        case UV_DIRENT_LINK:
            return DirectoryEntryType::LINK;
        case UV_DIRENT_FIFO:
            return DirectoryEntryType::FIFO;
        case UV_DIRENT_SOCKET:
            return DirectoryEntryType::SOCKET;
        case UV_DIRENT_CHAR:
            return DirectoryEntryType::CHAR;
        case UV_DIRENT_BLOCK:
            return DirectoryEntryType::BLOCK;
        case UV_DIRENT_UNKNOWN:
        default:
            return DirectoryEntryType::UNKNOWN;
    }
}

} // namespace

Dir::Impl::Impl(AllocationContext& context, Dir& parent) :
    m_parent(&parent),
    m_loop(&context.loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(context.loop.raw_loop())) {
    memset(&m_open_dir_req, 0, sizeof(m_open_dir_req));
}

Dir::Impl::~Impl() {
    assert(m_read_request == nullptr && "Leaked m_read_request");
}

const Path& Dir::Impl::path() const {
    return m_path;
}

Dir::Impl::State Dir::Impl::state() const {
    return m_state;
}

void Dir::Impl::open(const Path& path, const OpenCallback& callback) {
    const bool continue_open = detail::open(*m_loop, *this, path, callback);
    if  (!continue_open) {
        return;
    }
    
    LOG_DEBUG(m_loop, "path:", path);
    m_path = path;
    m_open_callback = callback;
    m_open_dir_req.data = this;

    m_state = State::OPENING;

    uv_fs_opendir(m_uv_loop, &m_open_dir_req, path.string().c_str(), on_open_dir);
}

template<void(*UvCallback)(uv_fs_t*), typename ListCallback>
void Dir::Impl::list(const ListCallback& list_callback, const EndListCallback& end_list_callback) {
    if (!is_open()) {
        if (end_list_callback) {
            m_loop->schedule_callback([=](EventLoop&) { end_list_callback(*this->m_parent, StatusCode::DIR_NOT_OPEN); });
        }
        return;
    }

    if (m_read_request) {
        if (end_list_callback) {
            end_list_callback(*this->m_parent, StatusCode::OPERATION_ALREADY_IN_PROGRESS);
        }
        return;
    }

    m_read_request = new ReadDirRequest<ListCallback>(list_callback, end_list_callback);
    m_read_request->data = this;
    const Error read_dir_error = uv_fs_readdir(m_uv_loop, m_read_request, m_uv_dir, UvCallback);
    if (read_dir_error) {
        if (end_list_callback) {
            m_loop->schedule_callback([=](EventLoop&) { end_list_callback(*this->m_parent, read_dir_error); });
        }
        return;
    }
}

bool Dir::Impl::is_open() const {
    return m_uv_dir != nullptr;
}

void Dir::Impl::close(const CloseCallback& callback) {
    if (!is_open()) {
        return;
    }

    if (m_read_request) { // TODO: if read_in_progress()
        m_state = State::WANTCLOSE;
        m_loop->schedule_callback([this, callback](EventLoop&) {
            close(callback);
        });
        return;
    }

    if (m_state == State::CLOSING) {
       if (callback) {
           callback(*m_parent, StatusCode::OPERATION_ALREADY_IN_PROGRESS);
       }
       return;
    }

    m_state = State::CLOSING;

    uv_dir_t* dir = reinterpret_cast<uv_dir_t*>(m_open_dir_req.ptr);

    uv_fs_req_cleanup(&m_open_dir_req);

    m_open_dir_req.data = nullptr;

    auto closedir_req = new CloseDirRequest;
    closedir_req->close_callback = callback;
    closedir_req->data = this;
    uv_fs_closedir(m_uv_loop, closedir_req, dir, on_close_dir);
}

bool Dir::Impl::schedule_removal() {
    LOG_TRACE(m_loop, "path:", m_path);

    if (is_open()) {
        close([this](Dir&, const Error&) {
            this->m_parent->schedule_removal();
        });
        return false;
    }

    return true;
}

////////////////////////////////////////////// static //////////////////////////////////////////////
void Dir::Impl::on_open_dir(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<Dir::Impl*>(req->data);

    Error error(req->result);

    if (error) {
        uv_fs_req_cleanup(&this_.m_open_dir_req);

        LOG_ERROR(this_.m_loop, "Failed to open dir:", req->path ? req->path : "");
    } else {
        this_.m_uv_dir = reinterpret_cast<uv_dir_t*>(req->ptr);
        this_.m_uv_dir->dirents = this_.m_dirents;
        this_.m_uv_dir->nentries = Dir::Impl::DIRENTS_NUMBER;
    }

    this_.m_state = State::OPENED;

    if (this_.m_open_callback) {
        this_.m_open_callback(*this_.m_parent, error);
    }

    if (error) {
        this_.m_path.clear();
    }
}

void Dir::Impl::on_read_dir_no_continuation(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<Dir::Impl*>(req->data);
    auto& request = *reinterpret_cast<ReadDirRequest<Dir::ListEntryCallback>*>(req);

    uv_dir_t* dir = reinterpret_cast<uv_dir_t*>(req->ptr);

    if (req->result <= 0) {
        this_.m_read_request = nullptr;
        if (request.end_list_callback) {
            request.end_list_callback(*this_.m_parent, Error(req->result));
        }

        delete &request;
    } else {
        if (request.list_callback) {
            request.list_callback(*this_.m_parent, this_.m_dirents[0].name, convert_direntry_type(this_.m_dirents[0].type));
        }

        uv_fs_req_cleanup(&request); // cleaning up previous request

        if (this_.m_state != State::WANTCLOSE) {
            // Not handling return value because all data is inited and not NULL at this point
            uv_fs_readdir(req->loop, &request, dir, on_read_dir_no_continuation);
        } else {
            this_.m_read_request = nullptr;
            if (request.end_list_callback) {
                request.end_list_callback(*this_.m_parent, Error(req->result));
            }

            delete &request;
        }
    }
}

void Dir::Impl::on_read_dir_with_continuation(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<Dir::Impl*>(req->data);
    auto& request = *reinterpret_cast<ReadDirRequest<Dir::ListEntryWithContinuationCallback>*>(req);

    uv_dir_t* dir = reinterpret_cast<uv_dir_t*>(req->ptr);

    if (req->result <= 0) {
        this_.m_read_request = nullptr;
        if (request.end_list_callback) {
            request.end_list_callback(*this_.m_parent, Error(req->result));
        }

        delete &request;
    } else {
        Continuation continuation;
        if (request.list_callback) {
            request.list_callback(*this_.m_parent,
                                  this_.m_dirents[0].name,
                                  convert_direntry_type(this_.m_dirents[0].type),
                                  continuation);
        }

        if (!continuation.proceed()) {
            this_.m_read_request = nullptr;
            uv_fs_req_cleanup(&request);

            if (request.end_list_callback) {
                request.end_list_callback(*this_.m_parent, Error(req->result));
            }

            delete &request;
        } else {
            uv_fs_req_cleanup(&request); // cleaning up previous request
            // Not handling return value because all data is inited and not NULL at this point
            uv_fs_readdir(req->loop, &request, dir, on_read_dir_with_continuation);
        }
    }
}

void Dir::Impl::on_close_dir(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<Dir::Impl*>(req->data);
    auto& request = *reinterpret_cast<CloseDirRequest*>(req);

    LOG_TRACE(this_.m_loop, this_.m_parent);
    this_.m_uv_dir = nullptr;
    this_.m_path.clear();

    this_.m_state = State::CLOSED;

    Error error(req->result);
    if (request.close_callback) {
        request.close_callback(*this_.m_parent, error);
    }

    delete &request;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

Dir::Dir(AllocationContext& context) :
    Removable(context.loop),
    m_impl(new Impl(context, *this)) {
}

Dir::~Dir() {
}

void Dir::open(const Path& path, const OpenCallback& callback) {
    return m_impl->open(path, callback);
}

bool Dir::is_open() const {
    return m_impl->is_open();
}

void Dir::close(const CloseCallback& callback) {
    return m_impl->close(callback);
}

void Dir::list(const ListEntryCallback& list_callback, const EndListCallback& end_list_callback) {
    // A bit ugly approach with UV callback as template parameter, but GCC 4.8.4 has troubles with class
    // members partial specialization. And we can not use external dispatcher class because
    // Impl class is private member of Dir, so full qualified name will be under private section.
    return m_impl->list<Dir::Impl::on_read_dir_no_continuation>(list_callback, end_list_callback);
}

void Dir::list(const ListEntryWithContinuationCallback& list_callback, const EndListCallback& end_list_callback) {
    return m_impl->list<Dir::Impl::on_read_dir_with_continuation>(list_callback, end_list_callback);
}

const Path& Dir::path() const {
    return m_impl->path();
}

void Dir::schedule_removal() {
    Removable::set_removal_scheduled();
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

/////////////////////////////////////////// functions //////////////////////////////////////////////

template<typename CallbackType>
struct RequestWithCallback : public uv_fs_t {
    RequestWithCallback(CallbackType c) :
        callback(c) {
    }

    CallbackType callback = nullptr;
};

void on_make_temp_dir(uv_fs_t* uv_request) {
    auto& request = *reinterpret_cast<RequestWithCallback<MakeTempDirCallback>*>(uv_request);

    if (request.callback) {
        request.callback(request.path, Error(request.result));
    }

    uv_fs_req_cleanup(uv_request);
    delete &request;
}

void make_temp_dir(EventLoop& loop, const Path& name_template, const MakeTempDirCallback& callback) {
    auto request = new RequestWithCallback<MakeTempDirCallback>(callback);
    const Error error = uv_fs_mkdtemp(reinterpret_cast<uv_loop_t*>(loop.raw_loop()), request, name_template.string().c_str(), on_make_temp_dir);
    if (error) {
        if (callback) {
            callback("", error);
        }

        delete request;
    }
}

void on_make_dir(uv_fs_t* uv_request) {
    auto& request = *reinterpret_cast<RequestWithCallback<MakeDirCallback>*>(uv_request);

    if (request.callback) {
        Error error = request.result;

#ifdef _MSC_VER
        // Workaround.
        // CreateDirectory function on Windows returns only 2 values on error
        // ERROR_ALREADY_EXISTS and ERROR_PATH_NOT_FOUND
        // to make bahavior consistent between platforms we handle case of long name errors manually
        if (error && strlen(request.path) + 1 > MAX_PATH) { // +1 is for 0 terminating char
            error = Error(StatusCode::NAME_TOO_LONG);
        }
#endif // _MSC_VER

        request.callback(request.path, error);
    }

    uv_fs_req_cleanup(uv_request);
    delete &request;
}

void make_dir_impl(EventLoop& loop, const Path& path, int mode, const MakeDirCallback& callback) {
    if (path.empty()) {
        if (callback) {
            callback(path, StatusCode::INVALID_ARGUMENT);
        }
        return;
    }

    if (path.root_name() == path || path.root_directory() == path) {
        if (callback) {
            callback(path, StatusCode::ILLEGAL_OPERATION_ON_A_DIRECTORY);
        }
        return;
    }

    auto request = new RequestWithCallback<MakeDirCallback>(callback);
    const Error error =
        uv_fs_mkdir(reinterpret_cast<uv_loop_t*>(loop.raw_loop()), request, path.string().c_str(), mode, on_make_dir);
    if (error) {
        if (callback) {
            callback(path, error);
        }

        delete request;
    }
}

void make_dir(EventLoop& loop, const Path& path, int mode, const MakeDirCallback& callback) {
    ::tarm::io::detail::defer_execution_if_required(loop, make_dir_impl, loop, path, mode, callback);
}

void make_all_dirs_impl(EventLoop& loop, const Path& path, int mode, const MakeDirCallback& callback) {
    if (path.empty()) {
        if (callback) {
            callback(path, StatusCode::INVALID_ARGUMENT);
        }
        return;
    }

    const auto normalized_path = path.lexically_normal();

    // TODO: describe this approach in docs, find something better?
    // Post reply here https://stackoverflow.com/questions/2067988/recursive-lambda-functions-in-c11
    auto on_stat = new std::function<void(const Path&, const StatData&, const Error&)>();
    auto on_end = [&loop, on_stat, callback](const Path& path, const Error& error) {
        callback(path, error);
        loop.schedule_callback([on_stat](EventLoop&) {
            delete on_stat;
        });
    };

    *on_stat = [&loop, on_stat, normalized_path, mode, on_end](const Path& p, const StatData& stat_data, const Error& error) {
        auto it1 = normalized_path.begin();
        auto it2 = p.begin();
        while (it2 != p.end()) {
            ++it1;
            ++it2;
        }

        const auto next_path = (p / (*it1));

        if (error == StatusCode::NO_SUCH_FILE_OR_DIRECTORY) {
            make_dir(loop, p, mode,
                [&loop, on_stat, normalized_path, next_path, on_end](const Path& p, const Error& error) {
                    if (error) {
                        on_end(p, Error(error.code(), p.string()));
                        return;
                    }

                    if (p.size() != normalized_path.size()) {
                        stat(loop, next_path, *on_stat);
                    } else {
                        on_end(p, StatusCode::OK);
                    }
                }
            );
        } else if (error) {
            on_end(p, Error(error.code(), p.string()));
        } else {
            if (is_regular_file(stat_data)) {
                on_end(p, Error(StatusCode::NOT_A_DIRECTORY, p.string()));
                return;
            }

            if (p.size() != normalized_path.size()) {
                stat(loop, next_path, *on_stat);
            } else {
                on_end(p, StatusCode::OK);
            }
        }
    };

    stat(loop, *path.begin(), *on_stat);
}

void make_all_dirs(EventLoop& loop, const Path& path, int mode, const MakeDirCallback& callback) {
    ::tarm::io::detail::defer_execution_if_required(loop, make_all_dirs_impl, loop, path, mode, callback);
}

namespace {

struct RemoveDirWorkEntry {
    RemoveDirWorkEntry(const Path& p) :
        path(p),
        processed(false) {
    }

    Path path;
    bool processed;
};

struct RemoveDirStatusContext {
    RemoveDirStatusContext(const Error& e, const Path& p) :
        error(e),
        path(p) {
    }

    Error error = 0;
    Path path;
};

} // namespace

using RemoveDirWorkData = std::vector<RemoveDirWorkEntry>;

RemoveDirStatusContext remove_dir_entry(uv_loop_t* uv_loop, const Path& path, Path subpath, RemoveDirWorkData& work_data) {
    work_data.back().processed = true;

    const Path open_path = path / subpath;
    uv_fs_t open_dir_req;
    Error open_error = uv_fs_opendir(uv_loop, &open_dir_req, open_path.string().c_str(), nullptr);
    if (open_error) {
        uv_fs_req_cleanup(&open_dir_req);
        return {open_error, open_path};
    }

    uv_dirent_t uv_dir_entry[1];
    auto uv_dir = reinterpret_cast<uv_dir_t*>(open_dir_req.ptr);
    uv_dir->dirents = &uv_dir_entry[0];
    uv_dir->nentries = 1;

    ScopeExitGuard open_req_guard([&open_dir_req, uv_loop, uv_dir](){
        uv_fs_t close_dir_req;
        // Status code of uv_fs_closedir here could be ignored because we guarantee that uv_dir is not nullptr
        uv_fs_closedir(uv_loop, &close_dir_req, uv_dir, nullptr);

        uv_fs_req_cleanup(&close_dir_req);
        uv_fs_req_cleanup(&open_dir_req);
    });

    uv_fs_t read_dir_req;
    int entries_count = 0;

    do {
        ScopeExitGuard read_req_guard([&read_dir_req]() { uv_fs_req_cleanup(&read_dir_req); });

        entries_count = uv_fs_readdir(uv_loop, &read_dir_req, uv_dir, nullptr);
        if (entries_count > 0) {
            auto& entry = uv_dir_entry[0];

            if (entry.type != UV_DIRENT_DIR) {
                uv_fs_t unlink_request;
                const Path unlink_path = path / subpath / entry.name;
                Error unlink_error = uv_fs_unlink(uv_loop, &unlink_request, unlink_path.string().c_str(), nullptr);
                if (unlink_error) {
                    return {unlink_error, unlink_path};
                }
            } else {
                work_data.emplace_back(subpath / entry.name);
            }
        } else if (entries_count < 0) {
            return {entries_count, path};
        }
    } while (entries_count > 0);

    return {Error(0), ""};
}

void remove_dir_impl(EventLoop& loop, const Path& path, const RemoveDirCallback& remove_callback, const ProgressCallback& progress_callback) {
    loop.add_work([path, progress_callback](EventLoop& loop) -> void* {
        auto uv_loop = reinterpret_cast<uv_loop_t*>(loop.raw_loop());

        RemoveDirWorkData work_data;
        work_data.emplace_back(""); // Current directory

        do {
            const auto& status_context = remove_dir_entry(uv_loop, path, work_data.back().path, work_data);
            if (status_context.error) {
                return new RemoveDirStatusContext(status_context);
            }

            auto& last_entry = work_data.back();
            if (last_entry.processed) {
                const Path rmdir_path = path / last_entry.path;
                uv_fs_t rm_dir_req;
                Error rmdir_error = uv_fs_rmdir(uv_loop, &rm_dir_req, rmdir_path.string().c_str(), nullptr);
                if (rmdir_error) {
                    return new RemoveDirStatusContext(rmdir_error, rmdir_path);
                } else {
                    if (progress_callback) {
                        loop.execute_on_loop_thread([progress_callback, rmdir_path](EventLoop&){
                            progress_callback(rmdir_path.string());
                        });
                    }
                }

                work_data.pop_back();
            }
        } while(!work_data.empty());

        return new RemoveDirStatusContext(Error(0), "");
    },
    [remove_callback](EventLoop&, void* user_data, const Error& error) {
        auto& status_context = *reinterpret_cast<RemoveDirStatusContext*>(user_data);

        if (error) {
            remove_callback(error);
        } else {
            remove_callback(status_context.error);
        }

        delete &status_context;
    });
}

void remove_dir(EventLoop& loop, const Path& path, const RemoveDirCallback& remove_callback, const ProgressCallback& progress_callback) {
    ::tarm::io::detail::defer_execution_if_required(loop, remove_dir_impl, loop, path, remove_callback, progress_callback);
}

} // namespace fs
} // namespace io
} // namespace tarm

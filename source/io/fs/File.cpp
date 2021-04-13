/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "File.h"

#include "detail/Common.h"
#include "detail/LogMacros.h"
#include "detail/EventLoopHelpers.h"
#include "fs/detail/FsCommon.h"
#include "ScopeExitGuard.h"

namespace tarm {
namespace io {
namespace fs {

namespace {

struct FileReadRequest : public uv_fs_t {
    FileReadRequest() {
        // This memset is needed while we stor request by value
        memset(reinterpret_cast<uv_fs_t*>(this), 0, sizeof(uv_fs_t));
    }

    ~FileReadRequest() {
        delete[] raw_buf;
    }

    std::shared_ptr<char> buf;

    bool is_free = true;
    char* raw_buf = nullptr;
};

struct FileCloseRequest : public uv_fs_t {
    File::CloseCallback close_callback;
};

struct FileReadBlockReq : public uv_fs_t {
    std::shared_ptr<char> buf;
    std::size_t offset = 0;
};

struct FileStatRequest : public uv_fs_t {
    File::StatCallback callback;
};

} // namespace

class File::Impl {
public:
    using ParentType = File;

    enum class State {
        INITIAL = 0,
        OPENING,
        OPENED,
        CLOSING,
        CLOSED
    };

    Impl(EventLoop& loop, Error& error, File& parent);
    ~Impl();

    void open(const Path& path, const OpenCallback& callback);
    void close(const CloseCallback& close_callback);
    bool is_open() const;

    void read(const ReadCallback& callback);
    void read(const ReadCallback& read_callback, const EndReadCallback& end_read_callback);

    void read_block(off_t offset, unsigned int bytes_count, const ReadCallback& read_callback);

    const Path& path() const;

    void stat(const StatCallback& callback);

    bool schedule_removal();

    State state() const;

protected:
    // statics
    static void on_open(uv_fs_t* req);
    static void on_read(uv_fs_t* req);
    static void on_close(uv_fs_t* req);
    static void on_read_block(uv_fs_t* req);
    static void on_stat(uv_fs_t* req);

private:
    void schedule_read();
    void schedule_read(FileReadRequest& req);
    bool has_read_buffers_in_use() const;
    void finish_close();

    OpenCallback m_open_callback = nullptr;
    ReadCallback m_read_callback = nullptr;
    EndReadCallback m_end_read_callback = nullptr;

    File* m_parent = nullptr;

    EventLoop* m_loop = nullptr;
    uv_loop_t* m_uv_loop = nullptr;

    FileReadRequest m_read_reqs[READ_BUFS_NUM];
    std::size_t m_current_offset = 0;

    bool m_read_in_progress = false;
    bool m_done_read = false;
    bool m_need_reschedule_remove = false;

    uv_fs_t* m_open_request = nullptr;
    uv_file m_file_handle = -1;
    FileStatRequest* m_stat_req = nullptr;

    Path m_path;

    State m_state = State::INITIAL;
};

File::Impl::Impl(EventLoop& loop, Error& /*error*/, File& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())) {
}

File::Impl::~Impl() {
    LOG_TRACE(m_loop, "");

    for (size_t i = 0; i < READ_BUFS_NUM; ++i) {
        uv_fs_req_cleanup(&m_read_reqs[i]);
    }
}

File::Impl::State File::Impl::state() const {
    return m_state;
}

bool File::Impl::schedule_removal() {
    LOG_TRACE(m_loop, "path:", m_path);

    if (is_open()) {
        close([this](File&, const Error&) {
            if (has_read_buffers_in_use()) {
                m_need_reschedule_remove = true;
                LOG_TRACE(m_loop, "File has read buffers in use, postponing removal");
            } else {
                this->m_parent->schedule_removal();
            }
        });
        return false;
    } else if (has_read_buffers_in_use()) {
        m_need_reschedule_remove = true;
        LOG_TRACE(m_loop, "File has read buffers in use, postponing removal");
        return false;
    }


    return true;
}

bool File::Impl::is_open() const {
    return m_file_handle != -1;
}

void File::Impl::close(const CloseCallback& close_callback) {
    if (!is_open()) {
        if (close_callback) {
            m_loop->schedule_callback([this, close_callback](EventLoop&) {
                close_callback(*this->m_parent, StatusCode::FILE_NOT_OPEN);
            });
        }
        return;
    }

    if (m_state == State::CLOSING) {
        if (close_callback) {
            close_callback(*m_parent, StatusCode::OPERATION_ALREADY_IN_PROGRESS);
        }
        return;
    }

    m_state = State::CLOSING;

    LOG_DEBUG(m_loop, "path: ", m_path);

    auto close_req = new FileCloseRequest;
    close_req->data = this;
    close_req->close_callback = close_callback;
    Error close_error = uv_fs_close(m_uv_loop, close_req, m_file_handle, on_close);
    if (close_error) {
        LOG_ERROR(m_loop, "Error:", close_error);
        m_file_handle = -1;
        if (close_callback) {
            m_loop->schedule_callback([=](EventLoop&) {
                close_callback(*m_parent, close_error);

                if (m_state == State::CLOSED) {
                    finish_close();
                }
            });
        }
    }
}

void File::Impl::finish_close() {
    // Setting request data to nullptr to allow any pending callback exit properly
    if (m_open_request) {
        m_open_request->data = nullptr;
        m_open_request = nullptr;
    }

    m_path.clear();
}

void File::Impl::open(const Path& path, const OpenCallback& callback) {
    const bool continue_open = detail::open(*m_loop, *this, path, callback);
    if  (!continue_open) {
        return;
    }

    LOG_DEBUG(m_loop, "path:", path);

    m_state = State::OPENING;

    m_path = path;
    m_current_offset = 0;
    m_open_request = new uv_fs_t;
    std::memset(m_open_request, 0, sizeof(uv_fs_t));
    m_open_callback = callback;
    m_open_request->data = this;
    uv_fs_open(m_uv_loop, m_open_request, path.string().c_str(), UV_FS_O_RDWR, 0, on_open);
}

void File::Impl::read(const ReadCallback& read_callback, const EndReadCallback& end_read_callback) {
    if (!is_open()) {
        if (read_callback) {
            ::tarm::io::detail::defer_execution_if_required(*m_loop,
                [&, read_callback](){ read_callback(*m_parent, DataChunk(), Error(StatusCode::FILE_NOT_OPEN)); });
        }

        return;
    }

    m_read_callback = read_callback;
    m_end_read_callback = end_read_callback;
    m_done_read = false;

    schedule_read();
}

void File::Impl::read_block(off_t offset, unsigned int bytes_count, const ReadCallback& read_callback) {
    if (!is_open()) {
        if (read_callback) {
            read_callback(*m_parent, DataChunk(), Error(StatusCode::FILE_NOT_OPEN));
        }

        return;
    }

    m_read_callback = read_callback;

    auto req = new FileReadBlockReq;
    req->buf = std::shared_ptr<char>(new char[bytes_count], [](char* p) {delete[] p;});
    req->data = this;
    req->offset = offset;

    uv_buf_t buf = uv_buf_init(req->buf.get(), bytes_count);

    const Error read_error = uv_fs_read(m_uv_loop, req, m_file_handle, &buf, 1, offset, on_read_block);
    if (read_error) {
        if (read_callback) {
            m_loop->schedule_callback([this, read_callback, read_error](io::EventLoop&) {
                DataChunk data;
                read_callback(*this->m_parent, data, read_error);
            });
        }
    }
}

void File::Impl::read(const ReadCallback& callback) {
    read(callback, nullptr);
}

void File::Impl::stat(const StatCallback& callback) {
    if (!is_open()) {
        if (callback) {
            m_loop->schedule_callback([=](EventLoop&) {
                StatData data;
                callback(*this->m_parent, data, StatusCode::FILE_NOT_OPEN);
            });
        }
        return;
    }

    if (m_stat_req) {
        if (callback) {
            m_loop->schedule_callback([=](EventLoop&) {
                StatData data;
                callback(*this->m_parent, data, StatusCode::OPERATION_ALREADY_IN_PROGRESS);
            });
        }
        return;
    }

    m_stat_req = new FileStatRequest;
    m_stat_req->data = this;
    m_stat_req->callback = callback;

    uv_fs_fstat(m_uv_loop, m_stat_req, m_file_handle, on_stat);
}

const Path& File::Impl::path() const {
    return m_path;
}

void File::Impl::schedule_read() {
    if (!is_open()) {
        return;
    }

    if (m_parent->is_removal_scheduled()) {
        return;
    }

    if (m_state == State::CLOSING) {
        return;
    }

    if (m_read_in_progress) {
        return;
    }

    size_t i = 0;
    bool found_free_buffer = false;
    for (; i < READ_BUFS_NUM; ++i) {
        if (m_read_reqs[i].is_free) {
            found_free_buffer = true;
            break;
        }
    }

    if (!found_free_buffer) {
        LOG_TRACE(m_loop, "File", m_path, "no free buffer found");
        return;
    }

    LOG_TRACE(m_loop, "File", m_path, "using buffer with index: ", i);

    FileReadRequest& read_req = m_read_reqs[i];
    read_req.is_free = false;
    read_req.data = this;

    schedule_read(read_req);
}

void File::Impl::schedule_read(FileReadRequest& req) {
    m_read_in_progress = true;
    m_loop->start_block_loop_from_exit();

    if (req.raw_buf == nullptr) {
        req.raw_buf = new char[READ_BUF_SIZE];
    }

    // Custom deleter here is used to inform that nobody externally is using pointer and we can
    // safely continue reading or remove the object.
    req.buf = std::shared_ptr<char>(req.raw_buf, [this, &req](const char* /*p*/) {
        LOG_TRACE(this->m_loop, this->m_path, "buffer freed");

        req.is_free = true;
        m_loop->stop_block_loop_from_exit();

        if (this->m_need_reschedule_remove) {
            if (!has_read_buffers_in_use()) {
                m_parent->schedule_removal();
            }

            return;
        }

        if (m_done_read) {
            return;
        }

        LOG_TRACE(this->m_loop, this->m_path, "schedule_read again");
        schedule_read();
    });

    uv_buf_t buf = uv_buf_init(req.buf.get(), READ_BUF_SIZE);
    Error read_error = uv_fs_read(m_uv_loop, &req, m_file_handle, &buf, 1, -1, on_read);
    if (read_error)  {
        ::tarm::io::detail::defer_execution_if_required(*m_loop,
            [read_error, this](){
                this->m_read_in_progress = false;
                DataChunk chunk;
                m_read_callback(*this->m_parent, chunk, read_error);
            }
        );
    }
}

bool File::Impl::has_read_buffers_in_use() const {
    for (size_t i = 0; i < READ_BUFS_NUM; ++i) {
        if (!m_read_reqs[i].is_free) {
            return true;
        }
    }

    return false;
}

////////////////////////////////////////////// static //////////////////////////////////////////////
void File::Impl::on_open(uv_fs_t* req) {
    ScopeExitGuard on_scope_exit([req]() {
        if (req->data) {
            reinterpret_cast<File::Impl*>(req->data)->m_open_request = nullptr;
        }

        uv_fs_req_cleanup(req);
        delete req;
    });

    if (req->data == nullptr) {
        return;
    }

    auto& this_ = *reinterpret_cast<File::Impl*>(req->data);

    Error error(req->result > 0 ? 0 : req->result);

    if (error) {
        if (this_.m_open_callback) {
            this_.m_open_callback(*this_.m_parent, error);
        }
        this_.m_path.clear();
        this_.m_state = State::CLOSED;
        return;
    } else {
        this_.m_file_handle = static_cast<uv_file>(req->result);
    }

#ifdef TARM_IO_PLATFORM_WINDOWS
    // Making stat here because Windows allows open directory as a file, so the call is successful on this
    // platform. But Linux and Mac return error immediately.
    // This should not be perfromance bottleneck because filesystem meta-data already resides in OS cache.
    this_.stat([&this_](File& f, const StatData& d, const Error& error) {
        if (error) {
            if (this_.m_open_callback) {
                this_.m_open_callback(*this_.m_parent, error);
                this_.m_path.clear();
                this_.m_state = State::CLOSED;
            }
            return;
        }

        if (this_.m_open_callback) {
            if (d.mode & S_IFDIR) {
                this_.m_loop->schedule_callback([&this_](EventLoop&){
                    this_.m_open_callback(*this_.m_parent, StatusCode::ILLEGAL_OPERATION_ON_A_DIRECTORY);
                    this_.m_path.clear();
                    this_.m_state = State::CLOSED;
                });
            }
            else {
                this_.m_loop->schedule_callback([&this_](EventLoop&){
                    this_.m_state = State::OPENED;
                    this_.m_open_callback(*this_.m_parent, StatusCode::OK);
                });
            }
        }
    });
#else
    if (this_.m_open_callback) {
        this_.m_state = State::OPENED;
        this_.m_open_callback(*this_.m_parent, StatusCode::OK);
    }
#endif
}

void File::Impl::on_read_block(uv_fs_t* uv_req) {
    auto& req = *reinterpret_cast<FileReadBlockReq*>(uv_req);
    auto& this_ = *reinterpret_cast<File::Impl*>(req.data);

    ScopeExitGuard req_guard([&req]() {
        delete &req;
    });

    if (!this_.is_open()) {
        if (this_.m_read_callback) {
            this_.m_read_callback(*this_.m_parent, DataChunk(), Error(StatusCode::FILE_NOT_OPEN));
        }

        return;
    }

    Error error(req.result);
    if (this_.m_read_callback) {
        if (req.result < 0) {
            LOG_ERROR(this_.m_loop, "File:", this_.m_path, error);
            DataChunk data_chunk;
            this_.m_read_callback(*this_.m_parent, data_chunk, error);
        } else if (req.result > 0) {
            DataChunk data_chunk(req.buf, req.result, req.offset);
            this_.m_read_callback(*this_.m_parent, data_chunk, error);
        }
    }
}

void File::Impl::on_read(uv_fs_t* uv_req) {
    auto& req = *reinterpret_cast<FileReadRequest*>(uv_req);
    auto& this_ = *reinterpret_cast<File::Impl*>(req.data);

    if (!this_.is_open()) {
        req.buf.reset();

        if (this_.m_read_callback) {
            this_.m_read_callback(*this_.m_parent, DataChunk(), Error(StatusCode::FILE_NOT_OPEN));
        }

        return;
    }

    this_.m_read_in_progress = false;

    if (req.result < 0) {
        this_.m_done_read = true;
        req.buf.reset();

        LOG_ERROR(this_.m_loop, "File:", this_.m_path, "read error:", uv_strerror(static_cast<int>(req.result)));

        if (this_.m_read_callback) {
            Error error(req.result);
            this_.m_read_callback(*this_.m_parent, DataChunk(), error);
        }
    } else if (req.result == 0) {
        this_.m_done_read = true;

        if (this_.m_end_read_callback) {
            this_.m_end_read_callback(*this_.m_parent);
        }

        req.buf.reset();

        //uv_print_all_handles(this_.m_uv_loop, stdout);
        //this_.m_loop->stop_block_loop_from_exit();
    } else if (req.result > 0) {
        if (this_.m_read_callback) {
            Error error(0);
            DataChunk data_chunk(req.buf, req.result, this_.m_current_offset);
            this_.m_read_callback(*this_.m_parent, data_chunk, error);
            this_.m_current_offset += req.result;
        }

        this_.schedule_read();
        req.buf.reset();
    }
}

void File::Impl::on_close(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<File::Impl*>(req->data);
    auto& request = *reinterpret_cast<FileCloseRequest*>(req);

    this_.m_file_handle = -1;
    this_.m_state = State::CLOSED;

    Error error(req->result);
    if (request.close_callback) {
        request.close_callback(*this_.m_parent, error);
    }

    if (this_.m_state == State::CLOSED) {
        this_.finish_close();
    }

    delete &request;
}

void File::Impl::on_stat(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<File::Impl*>(req->data);
    auto& request = *reinterpret_cast<FileStatRequest*>(req);

    this_.m_stat_req = nullptr;

    Error error(req->result);
    if (request.callback) {
        request.callback(*this_.m_parent, *reinterpret_cast<StatData*>(&request.statbuf), error);
    }

    delete &request;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////



File::File(EventLoop& loop, Error& error) :
    Removable(loop),
    m_impl(new Impl(loop, error, *this)) {
}

File::~File() {
}

void File::open(const Path& path, const OpenCallback& callback) {
    return m_impl->open(path, callback);
}

void File::close(const CloseCallback& close_callback) {
    return m_impl->close(close_callback);
}

bool File::is_open() const {
    return m_impl->is_open();
}

void File::read(const ReadCallback& callback) {
    return m_impl->read(callback);
}

void File::read(const ReadCallback& read_callback, const EndReadCallback& end_read_callback) {
    return m_impl->read(read_callback, end_read_callback);
}

void File::read_block(off_t offset, unsigned int bytes_count, const ReadCallback& read_callback) {
    return m_impl->read_block(offset, bytes_count, read_callback);
}

const Path& File::path() const {
    return m_impl->path();
}

void File::stat(const StatCallback& callback) {
    return m_impl->stat(callback);
}

void File::schedule_removal() {
    Removable::set_removal_scheduled();
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

} // namespace fs
} // namespace io
} // namespace tarm

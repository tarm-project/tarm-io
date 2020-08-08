/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "File.h"

#include "detail/Common.h"
#include "ScopeExitGuard.h"

namespace tarm {
namespace io {
namespace fs {

namespace {

struct ReadRequest : public uv_fs_t {
    ReadRequest() {
        memset(reinterpret_cast<uv_fs_t*>(this), 0, sizeof(uv_fs_t));
    }

    ~ReadRequest() {
        delete[] raw_buf;
    }

    std::shared_ptr<char> buf;

    bool is_free = true;
    char* raw_buf = nullptr;
};

struct CloseRequest : public uv_fs_t {
    CloseRequest() {
    }

    ~CloseRequest() {
    }

    File::CloseCallback close_callback;
};

} // namespace

struct ReadBlockReq : public uv_fs_t {
    std::shared_ptr<char> buf;
    std::size_t offset = 0;
};

class File::Impl {
public:
    enum class State {
        INITIAL = 0,
        OPENING,
        OPENED,
        CLOSING,
        CLOSED
    };

    Impl(EventLoop& loop, File& parent);
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

protected:
    // statics
    static void on_open(uv_fs_t* req);
    static void on_read(uv_fs_t* req);
    static void on_close(uv_fs_t* req);
    static void on_read_block(uv_fs_t* req);
    static void on_stat(uv_fs_t* req);

private:
    void schedule_read();
    void schedule_read(ReadRequest& req);
    bool has_read_buffers_in_use() const;
    void finish_close();

    OpenCallback m_open_callback = nullptr;
    ReadCallback m_read_callback = nullptr;
    EndReadCallback m_end_read_callback = nullptr;
    StatCallback m_stat_callback = nullptr;

    File* m_parent = nullptr;

    EventLoop* m_loop = nullptr;
    uv_loop_t* m_uv_loop = nullptr;

    ReadRequest m_read_reqs[READ_BUFS_NUM];
    std::size_t m_current_offset = 0;

    // TODO: make some states instead bunch of flags
    bool m_read_in_progress = false;
    bool m_done_read = false;
    bool m_need_reschedule_remove = false;

    // TODO: it may be not reasonable to store by value here because they take to much space (>400b)
    //       also memory pool will help a lot
    uv_fs_t* m_open_request = nullptr;
    uv_file m_file_handle = -1;
    uv_fs_t m_stat_req;
    uv_fs_t m_write_req;

    Path m_path;

    State m_state = State::INITIAL;
};

File::Impl::Impl(EventLoop& loop, File& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())) {

    memset(&m_write_req, 0, sizeof(m_write_req));
    memset(&m_stat_req, 0, sizeof(m_stat_req));
}

File::Impl::~Impl() {
    IO_LOG(m_loop, TRACE, "");

    uv_fs_req_cleanup(&m_write_req);

    for (size_t i = 0; i < READ_BUFS_NUM; ++i) {
        uv_fs_req_cleanup(&m_read_reqs[i]);
    }
}

bool File::Impl::schedule_removal() {
    IO_LOG(m_loop, TRACE, "path:", m_path);

    if (is_open()) {
        close([this](File&, const Error&) {
            if (has_read_buffers_in_use()) {
                m_need_reschedule_remove = true;
                IO_LOG(m_loop, TRACE, "File has read buffers in use, postponing removal");
            } else {
                this->m_parent->schedule_removal();
            }
        });
        return false;
    } else if (has_read_buffers_in_use()) {
        m_need_reschedule_remove = true;
        IO_LOG(m_loop, TRACE, "File has read buffers in use, postponing removal");
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

    IO_LOG(m_loop, DEBUG, "path: ", m_path);

    auto close_req = new CloseRequest;
    close_req->data = this;
    close_req->close_callback = close_callback;
    Error close_error = uv_fs_close(m_uv_loop, close_req, m_file_handle, on_close);
    if (close_error) {
        IO_LOG(m_loop, ERROR, "Error:", close_error);
        m_file_handle = -1;
        if (close_callback) {
            m_loop->schedule_callback([=](EventLoop&) {
                close_callback(*m_parent, close_error);
            });
        }
        finish_close();
    }
}

void File::Impl::finish_close() {
    // Setting request data to nullptr to allow any pending callback exit properly
    if (m_open_request) {
        m_open_request->data = nullptr;
        m_open_request = nullptr;
    }

    m_path.clear();
    m_state = State::CLOSED;
}

void File::Impl::open(const Path& path, const OpenCallback& callback) {
    if (is_open()) {
        close([=](File& file, const Error& error) {
            if (error) {
                if(callback) {
                    callback(file, error);
                }
            } else {
                this->m_loop->schedule_callback([=](EventLoop&) {
                    this->open(path, callback);
                });
            }
        });
        return;
    } else if (!(m_state == State::INITIAL || m_state == State::CLOSED)) {
        this->m_loop->schedule_callback([=](EventLoop&) {
            this->open(path, callback);
        });
        return;
    }

    IO_LOG(m_loop, DEBUG, "path:", path);

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
            read_callback(*m_parent, DataChunk(), Error(StatusCode::FILE_NOT_OPEN));
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

    auto req = new ReadBlockReq;
    req->buf = std::shared_ptr<char>(new char[bytes_count], [](char* p) {delete[] p;});
    req->data = this;
    req->offset = offset;

    uv_buf_t buf = uv_buf_init(req->buf.get(), bytes_count);
    // TODO: error handling for uv_fs_read return value
    uv_fs_read(m_uv_loop, req, m_file_handle, &buf, 1, offset, on_read_block);
}

void File::Impl::read(const ReadCallback& callback) {
    read(callback, nullptr);
}

void File::Impl::stat(const StatCallback& callback) {
    // TODO: check if open

    m_stat_req.data = this;
    m_stat_callback = callback;

    uv_fs_fstat(m_uv_loop, &m_stat_req, m_file_handle, on_stat);
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
        IO_LOG(m_loop, TRACE, "File", m_path, "no free buffer found");
        return;
    }

    IO_LOG(m_loop, TRACE, "File", m_path, "using buffer with index: ", i);

    ReadRequest& read_req = m_read_reqs[i];
    read_req.is_free = false;
    read_req.data = this;

    schedule_read(read_req);
}

void File::Impl::schedule_read(ReadRequest& req) {
    m_read_in_progress = true;
    m_loop->start_block_loop_from_exit();

    if (req.raw_buf == nullptr) {
        req.raw_buf = new char[READ_BUF_SIZE];
    }

    // TODO: comments on this shared pointer
    req.buf = std::shared_ptr<char>(req.raw_buf, [this, &req](const char* /*p*/) {
        IO_LOG(this->m_loop, TRACE, this->m_path, "buffer freed");

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

        IO_LOG(this->m_loop, TRACE, this->m_path, "schedule_read again");
        schedule_read();
    });

    uv_buf_t buf = uv_buf_init(req.buf.get(), READ_BUF_SIZE);
    // TODO: error handling for uv_fs_read return value
    uv_fs_read(m_uv_loop, &req, m_file_handle, &buf, 1, -1, on_read);
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

    // Making stat here because Windows allows open directory as a file, so the call is successful on this
    // platform. But Linux and Mac return errro immediately.
    this_.stat([&this_](File& f, const StatData& d) {
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
                    this_.m_open_callback(*this_.m_parent, StatusCode::OK);
                    this_.m_state = State::OPENED;
                });
            }
        }
    });
}

void File::Impl::on_read_block(uv_fs_t* uv_req) {
    auto& req = *reinterpret_cast<ReadBlockReq*>(uv_req);
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

    if (req.result < 0) {
        // TODO: error handling!

        IO_LOG(this_.m_loop, ERROR, "File:", this_.m_path, "read error:", uv_strerror(static_cast<int>(req.result)));
    } else if (req.result > 0) {
        if (this_.m_read_callback) {
            Error error(0);
            DataChunk data_chunk(req.buf, req.result, req.offset);
            this_.m_read_callback(*this_.m_parent, data_chunk, error);
        }
    }
}

void File::Impl::on_read(uv_fs_t* uv_req) {
    auto& req = *reinterpret_cast<ReadRequest*>(uv_req);
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

        IO_LOG(this_.m_loop, ERROR, "File:", this_.m_path, "read error:", uv_strerror(static_cast<int>(req.result)));

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
    auto& request = *reinterpret_cast<CloseRequest*>(req);

    this_.m_file_handle = -1;
    Error error(req->result);
    if (request.close_callback) {
        request.close_callback(*this_.m_parent, error);
    }

    this_.finish_close();

    delete &request;
}

void File::Impl::on_stat(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<File::Impl*>(req->data);

    Error error(req->result);
    // TODO: error handling (need to expand StatCallback)
    if (this_.m_stat_callback) {
        this_.m_stat_callback(*this_.m_parent, *reinterpret_cast<StatData*>(&this_.m_stat_req.statbuf));
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////



File::File(EventLoop& loop) :
    Removable(loop),
    m_impl(new Impl(loop, *this)) {
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

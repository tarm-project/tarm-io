#include "File.h"

#include "ScopeExitGuard.h"

// TODO: remove
#include <assert.h>

namespace io {

struct ReadRequest : public uv_fs_t {
    ReadRequest() {
        memset(this, 0, sizeof(uv_fs_t));
    }

    ~ReadRequest() {
        delete[] raw_buf;
    }

    std::shared_ptr<char> buf;

    bool is_free = true;
    char* raw_buf = nullptr;
};

class File::Impl {
public:
    Impl(EventLoop& loop, File& parent);
    ~Impl();

    void open(const std::string& path, OpenCallback callback);
    void close();
    bool is_open() const;

    void read(ReadCallback callback);
    void read(ReadCallback read_callback, EndReadCallback end_read_callback);

    void read_block(off_t offset, std::size_t bytes_count, ReadCallback read_callback);

    const std::string& path() const;

    void stat(StatCallback callback);

    bool schedule_removal();

protected:
    // statics
    static void on_open(uv_fs_t* req);
    static void on_read(uv_fs_t* req);
    static void on_read_block(uv_fs_t* req);
    static void on_stat(uv_fs_t* req);

private:
    void schedule_read();
    void schedule_read(ReadRequest& req);
    bool has_read_buffers_in_use() const;

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

    // TODO: it may be not reasonable to store by pointers here because they take to much space (>400b)
    //       also memory pool will help a lot
    uv_fs_t* m_open_request = nullptr;
    decltype(uv_fs_t::result) m_file_handle = -1;
    uv_fs_t m_stat_req;
    uv_fs_t m_write_req;

    std::string m_path;
};

File::Impl::Impl(EventLoop& loop, File& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())) {

    memset(&m_write_req, 0, sizeof(m_write_req));
    memset(&m_stat_req, 0, sizeof(m_stat_req));
}

File::Impl::~Impl() {
    IO_LOG(m_loop, TRACE, "File::~File");

    close();

    uv_fs_req_cleanup(&m_write_req);

    for (size_t i = 0; i < READ_BUFS_NUM; ++i) {
        uv_fs_req_cleanup(&m_read_reqs[i]);
    }
}

bool File::Impl::schedule_removal() {
    IO_LOG(m_loop, TRACE, "File::schedule_removal ", m_path);

    close();

    if (has_read_buffers_in_use()) {
        m_need_reschedule_remove = true;
        IO_LOG(m_loop, TRACE, "File has read buffers in use, postponing removal");
        return false;
    }

    return true;
}

bool File::Impl::is_open() const {
    return m_file_handle != -1;
}

void File::Impl::close() {
    if (!is_open()) {
        return;
    }

    IO_LOG(m_loop, DEBUG, "File::close ", m_path);

    uv_fs_t close_req;
    int status = uv_fs_close(m_uv_loop, &close_req, m_file_handle, nullptr);
    if (status != 0) {
        IO_LOG(m_loop, WARNING, "File::close status: ", uv_strerror(status));
        // TODO: error handing????
    }

    // Setting request data to nullptr to allow any pending callback exit properly
    m_file_handle = -1;
    if (m_open_request) {
        m_open_request->data = nullptr;
        m_open_request = nullptr;
    }

    //m_open_callback = nullptr;
    //m_read_callback = nullptr;
    //m_end_read_callback = nullptr;

    m_path.clear();
}

void File::Impl::open(const std::string& path, OpenCallback callback) {
    if (is_open()) {
        close();
    }

    IO_LOG(m_loop, DEBUG, "File::open ", path);

    m_path = path;
    m_current_offset = 0;
    m_open_request = new uv_fs_t;
    std::memset(m_open_request, 0, sizeof(uv_fs_t));
    m_open_callback = callback;
    m_open_request->data = this;
    uv_fs_open(m_uv_loop, m_open_request, path.c_str(), UV_FS_O_RDWR, 0, on_open);
}

void File::Impl::read(ReadCallback read_callback, EndReadCallback end_read_callback) {
    if (!is_open()) {
        if (read_callback) {
            read_callback(*m_parent, DataChunk(), Status(StatusCode::FILE_NOT_OPEN));
        }

        return;
    }

    m_read_callback = read_callback;
    m_end_read_callback = end_read_callback;
    m_done_read = false;

    schedule_read();
}

namespace {

struct ReadBlockReq : public uv_fs_t {
    ReadBlockReq() {
        // TODO: remove memset????
        memset(this, 0, sizeof(uv_fs_t));
    }

    ~ReadBlockReq() {
    }

    std::shared_ptr<char> buf;
    std::size_t offset = 0;
};

} // namespace

void File::Impl::read_block(off_t offset, std::size_t bytes_count, ReadCallback read_callback) {
    if (!is_open()) {
        if (read_callback) {
            read_callback(*m_parent, DataChunk(), Status(StatusCode::FILE_NOT_OPEN));
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

void File::Impl::read(ReadCallback callback) {
    read(callback, nullptr);
}

void File::Impl::stat(StatCallback callback) {
    // TODO: check if open

    m_stat_req.data = this;
    m_stat_callback = callback;

    uv_fs_fstat(m_uv_loop, &m_stat_req, m_file_handle, on_stat);
}

const std::string& File::Impl::path() const {
    return m_path;
}

void File::Impl::schedule_read() {
    //assert(m_used_read_bufs <= READ_BUFS_NUM);

    if (!is_open()) {
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
        IO_LOG(m_loop, TRACE, "File ", m_path, " no free buffer found");
        return;
    }

    IO_LOG(m_loop, TRACE, "File ", m_path, " using buffer with index: ", i);

    ReadRequest& read_req = m_read_reqs[i];
    read_req.is_free = false;
    read_req.data = this;

    schedule_read(read_req);
}

void File::Impl::schedule_read(ReadRequest& req) {
    m_read_in_progress = true;
    m_loop->start_dummy_idle();

    if (req.raw_buf == nullptr) {
        req.raw_buf = new char[READ_BUF_SIZE];
    }

    // TODO: comments on this shared pointer
    req.buf = std::shared_ptr<char>(req.raw_buf, [this, &req](const char* p) {
        IO_LOG(this->m_loop, TRACE, this->m_path, " buffer freed");

        req.is_free = true;
        m_loop->stop_dummy_idle();

        if (this->m_need_reschedule_remove) {
            if (!has_read_buffers_in_use()) {
                m_parent->schedule_removal();
            }

            return;
        }

        if (m_done_read) {
            return;
        }

        IO_LOG(this->m_loop, TRACE, this->m_path, " schedule_read");
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

    Status status(req->result > 0 ? 0 : req->result);
    if (status.ok()) {
        this_.m_file_handle = req->result;
    }

    if (this_.m_open_callback) {
        this_.m_open_callback(*this_.m_parent, status);
    }

    if (status.fail()) {
        this_.m_path.clear();
    }
}

void File::Impl::on_read_block(uv_fs_t* uv_req) {
    auto& req = *reinterpret_cast<ReadBlockReq*>(uv_req);
    auto& this_ = *reinterpret_cast<File::Impl*>(req.data);

    io::ScopeExitGuard req_guard([&req]() {
        delete &req;
    });

    if (!this_.is_open()) {
        if (this_.m_read_callback) {
            this_.m_read_callback(*this_.m_parent, DataChunk(), Status(StatusCode::FILE_NOT_OPEN));
        }

        return;
    }

    if (req.result < 0) {
        // TODO: error handling!

        IO_LOG(this_.m_loop, ERROR, "File: ", this_.m_path, " read error: ", uv_strerror(req.result));
    } else if (req.result > 0) {
        if (this_.m_read_callback) {
            io::Status status(0);
            DataChunk data_chunk(req.buf, req.result, req.offset);
            this_.m_read_callback(*this_.m_parent, data_chunk, status);
        }
    }
}

void File::Impl::on_read(uv_fs_t* uv_req) {
    auto& req = *reinterpret_cast<ReadRequest*>(uv_req);
    auto& this_ = *reinterpret_cast<File::Impl*>(req.data);

    if (!this_.is_open()) {
        req.buf.reset();

        if (this_.m_read_callback) {
            this_.m_read_callback(*this_.m_parent, DataChunk(), Status(StatusCode::FILE_NOT_OPEN));
        }

        return;
    }

    this_.m_read_in_progress = false;

    if (req.result < 0) {
        this_.m_done_read = true;
        req.buf.reset();

        IO_LOG(this_.m_loop, ERROR, "File: ", this_.m_path, " read error: ", uv_strerror(req.result));

        if (this_.m_read_callback) {
            io::Status status(req.result);
            this_.m_read_callback(*this_.m_parent, DataChunk(), status);
        }
    } else if (req.result == 0) {
        this_.m_done_read = true;

        if (this_.m_end_read_callback) {
            this_.m_end_read_callback(*this_.m_parent);
        }

        req.buf.reset();

        //uv_print_all_handles(this_.m_uv_loop, stdout);
        //this_.m_loop->stop_dummy_idle();
    } else if (req.result > 0) {
        if (this_.m_read_callback) {
            io::Status status(0);
            DataChunk data_chunk(req.buf, req.result, this_.m_current_offset);
            this_.m_read_callback(*this_.m_parent, data_chunk, status);
            this_.m_current_offset += req.result;
        }

        this_.schedule_read();
        req.buf.reset();
    }
}

void File::Impl::on_stat(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<File::Impl*>(req->data);

    if (this_.m_stat_callback) {
        this_.m_stat_callback(*this_.m_parent, *reinterpret_cast<StatData*>(&this_.m_stat_req.statbuf));
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

File::File(EventLoop& loop) :
    Disposable(loop),
    m_impl(new Impl(loop, *this)) {
}

File::~File() {
}

void File::open(const std::string& path, OpenCallback callback) {
    return m_impl->open(path, callback);
}

void File::close() {
    return m_impl->close();
}

bool File::is_open() const {
    return m_impl->is_open();
}

void File::read(ReadCallback callback) {
    return m_impl->read(callback);
}

void File::read(ReadCallback read_callback, EndReadCallback end_read_callback) {
    return m_impl->read(read_callback, end_read_callback);
}

void File::read_block(off_t offset, std::size_t bytes_count, ReadCallback read_callback) {
    return m_impl->read_block(offset, bytes_count, read_callback);
}

const std::string& File::path() const {
    return m_impl->path();
}

void File::stat(StatCallback callback) {
    return m_impl->stat(callback);
}

void File::schedule_removal() {
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Disposable::schedule_removal();
    }
}

} // namespace io

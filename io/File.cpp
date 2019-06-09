#include "File.h"

#include "ScopeExitGuard.h"

// TODO: remove
#include <iostream>
#include <assert.h>

namespace io {

File::File(EventLoop& loop) :
    Disposable(loop),
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())) {

    memset(&m_write_req, 0, sizeof(m_write_req));
    memset(&m_stat_req, 0, sizeof(m_stat_req));

    /*
    m_read_bufs.resize(READ_BUFS_NUM);
    for (size_t i = 0; i < READ_BUFS_NUM; ++i) {
        m_read_bufs[i].reset(new char[READ_BUF_SIZE]);
    }
    */
}

File::~File() {
    std::cout << "File::~File " << m_path << std::endl;
    close();

    //uv_fs_req_cleanup(&m_read_req);
    uv_fs_req_cleanup(&m_write_req);

    for (size_t i = 0; i < READ_BUFS_NUM; ++i) {
        uv_fs_req_cleanup(&m_read_reqs[i]);
    }
}

void File::schedule_removal() {
    std::cout << "File::schedule_removal " << m_path << std::endl;

    close();

    if (has_read_buffers_in_use()) {
        m_need_reschedule_remove = true;
        std::cout << "File " << m_path << " has read buffers in use, postponing removal" << std::endl;
        return;
    }

    Disposable::schedule_removal();
}

bool File::is_open() const {
    return m_file_handle != -1;
}

void File::close() {
    if (!is_open()) {
        return;
    }

    std::cout << "File::close " << m_path << std::endl;

    uv_fs_t close_req;
    int status = uv_fs_close(m_uv_loop, &close_req, m_file_handle, nullptr);
    if (status != 0) {
        std::cout << "File::close status: " << uv_strerror(status) << std::endl;
    }

    // Setting request data to nullptr to allow any pending callback exit properly
    m_file_handle = -1;
    if (m_open_request) {
        m_open_request->data = nullptr;
        m_open_request = nullptr;
    }

    m_open_callback = nullptr;
    m_read_callback = nullptr;
    m_end_read_callback = nullptr;

    m_path.clear();
}

void File::open(const std::string& path, OpenCallback callback) {
    if (is_open()) {
        close();
    }

    m_path = path;
    m_open_request = new uv_fs_t;
    std::memset(m_open_request, 0, sizeof(uv_fs_t));
    m_open_callback = callback;
    m_open_request->data = this;
    uv_fs_open(m_uv_loop, m_open_request, path.c_str(), UV_FS_O_RDWR, 0, File::on_open);
}

void File::read(ReadCallback read_callback, EndReadCallback end_read_callback) {
    if (!is_open()) {
        if (read_callback) {
            read_callback(*this, nullptr, 0, Status(StatusCode::FILE_NOT_OPEN));
        }
        return;
    }

    m_read_callback = read_callback;
    m_end_read_callback = end_read_callback;
    m_done_read = false;
    //m_read_req.data = this;
    //m_read_state = ReadState::CONTINUE;

    schedule_read();
}

namespace {

struct ReadBlockReq : public uv_fs_t {
    ReadBlockReq() {
        memset(this, 0, sizeof(uv_fs_t));
    }

    ~ReadBlockReq() {
    }

    std::shared_ptr<char> buf;
};

}

void File::read_block(off_t offset, std::size_t bytes_count, ReadCallback read_callback) {
    if (!is_open()) {
        if (read_callback) {
            read_callback(*this, nullptr, 0, Status(StatusCode::FILE_NOT_OPEN));
        }
        return;
    }

    m_read_callback = read_callback;

    auto req = new ReadBlockReq;
    req->buf = std::shared_ptr<char>(new char[bytes_count], [](char* p) {delete[] p;});
    req->data = this;

    uv_buf_t buf = uv_buf_init(req->buf.get(), bytes_count);
    // TODO: error handling for uv_fs_read return value
    uv_fs_read(m_uv_loop, req, m_file_handle, &buf, 1, offset, File::on_read_block);
}

void File::read(ReadCallback callback) {
    read(callback, nullptr);
}

void File::stat(StatCallback callback) {
    // TODO: check if open

    m_stat_req.data = this;
    m_stat_callback = callback;

    uv_fs_fstat(m_uv_loop, &m_stat_req, m_file_handle, File::on_stat);
}

const std::string& File::path() const {
    return m_path;
}

void File::schedule_read() {
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
        std::cout << "No free buffer found" << std::endl;
        //m_loop->start_dummy_idle();
        return;
    } else {
        //m_loop->stop_dummy_idle();
    }

    std::cout << "using buffer with index: " << i << std::endl;

    ReadReq& read_req = m_read_reqs[i];
    read_req.is_free = false;
    read_req.data = this;

    schedule_read(read_req);
}

void File::schedule_read(ReadReq& req) {
    m_read_in_progress = true;
    m_loop->start_dummy_idle();

    if (req.raw_buf == nullptr) {
        req.raw_buf = new char[READ_BUF_SIZE];
    }

    // TODO: comments on this shared pointer
    req.buf = std::shared_ptr<char>(req.raw_buf, [this, &req](const char* p) {
        req.is_free = true;
        m_loop->stop_dummy_idle();

        if (this->m_need_reschedule_remove) {
            if (!has_read_buffers_in_use()) {
                this->schedule_removal();
            }

            return;
        }

        if (m_done_read) {
            return;
        }

        schedule_read();
    });

    uv_buf_t buf = uv_buf_init(req.buf.get(), READ_BUF_SIZE);
    // TODO: error handling for uv_fs_read return value
    uv_fs_read(m_uv_loop, &req, m_file_handle, &buf, 1, -1, File::on_read);
}

bool File::has_read_buffers_in_use() const {
    for (size_t i = 0; i < READ_BUFS_NUM; ++i) {
        if (!m_read_reqs[i].is_free) {
            return true;
        }
    }

    return false;
}

/*
char* File::current_read_buf() {
    return m_read_bufs[m_current_read_buf_idx].get();
}

char* File::next_read_buf() {
    m_current_read_buf_idx = (m_current_read_buf_idx + 1) % READ_BUFS_NUM;
    return current_read_buf();
}

void File::pause_read() {
    if (m_read_state == ReadState::CONTINUE) {
        m_read_state = ReadState::PAUSE;
    }
}

void File::continue_read() {
    if (m_read_state == ReadState::PAUSE) {
        m_read_state = ReadState::CONTINUE;
        schedule_read();
    }
}

File::ReadState File::read_state() {
    return m_read_state;
}
*/
// ////////////////////////////////////// static //////////////////////////////////////
void File::on_open(uv_fs_t* req) {
    ScopeExitGuard on_scope_exit([req]() {
        if (req->data) {
            reinterpret_cast<File*>(req->data)->m_open_request = nullptr;
        }

        uv_fs_req_cleanup(req);
        delete req;
    });

    if (req->data == nullptr) {
        return;
    }

    auto& this_ = *reinterpret_cast<File*>(req->data);

    Status status(req->result > 0 ? 0 : req->result);
    if (status.ok()) {
        this_.m_file_handle = req->result;
    }

    if (this_.m_open_callback) {
        this_.m_open_callback(this_, status);
    }
}

void File::on_read_block(uv_fs_t* uv_req) {
    auto& req = *reinterpret_cast<ReadBlockReq*>(uv_req);
    auto& this_ = *reinterpret_cast<File*>(req.data);

    if (!this_.is_open()) {
        return;
    }

    if (req.result < 0) {
        // TODO: error handling!

        // TODO: remove this later
        std::cout << this_.path() << std::endl;
        fprintf(stderr, "File read error: %s\n",  uv_strerror(req.result));
    } else if (req.result > 0) {
        if (this_.m_read_callback) {
            io::Status status(0);
            this_.m_read_callback(this_, req.buf, req.result, status);
        }
    }

    delete &req;
}

void File::on_read(uv_fs_t* uv_req) {
    auto& req = *reinterpret_cast<ReadReq*>(uv_req);
    auto& this_ = *reinterpret_cast<File*>(req.data);

    if (!this_.is_open()) {
        req.buf.reset();
        return;
    }

    this_.m_read_in_progress = false;

    if (req.result < 0) {
        this_.m_done_read = true;
        req.buf.reset();

        if (this_.m_read_callback) {
            io::Status status(req.result);
            this_.m_read_callback(this_, nullptr, 0, status);
        }

        // TODO: remove this later
        std::cout << this_.path() << std::endl;
        fprintf(stderr, "File read error: %s\n",  uv_strerror(req.result));
    } else if (req.result == 0) {
        this_.m_done_read = true;
        req.buf.reset();

        if (this_.m_end_read_callback) {
            this_.m_end_read_callback(this_);
        }

        //uv_print_all_handles(this_.m_uv_loop, stdout);
        //this_.m_loop->stop_dummy_idle();
    } else if (req.result > 0) {
        if (this_.m_read_callback) {
            io::Status status(0);
            this_.m_read_callback(this_, req.buf, req.result, status);
        }

        this_.schedule_read();
        req.buf.reset();
    }
}

void File::on_close(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<File*>(req->data);

    if (this_.m_end_read_callback) {
        this_.m_end_read_callback(this_);
    }
}

void File::on_stat(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<File*>(req->data);

    if (this_.m_stat_callback) {
        this_.m_stat_callback(this_, *reinterpret_cast<io::Stat*>(&this_.m_stat_req.statbuf));
    }
}

} // namespace io

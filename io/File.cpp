#include "File.h"

// TODO: remove
#include <iostream>
#include <assert.h>

namespace io {

File::File(EventLoop& loop) :
    Disposable(loop),
    m_loop(&loop) {
    memset(&m_open_req, 0, sizeof(m_open_req));
    //memset(&m_read_req, 0, sizeof(m_read_req));
    memset(&m_write_req, 0, sizeof(m_write_req));

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

    uv_fs_req_cleanup(&m_open_req);
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
    return m_open_req.data == this;
}

void File::close() {
    if (!is_open()) {
        return;
    }

    std::cout << "File::close " << m_path << std::endl;

    //m_read_state = ReadState::DONE;

    m_open_req.data = nullptr;

    uv_fs_t close_req;
    int status = uv_fs_close(m_loop, &close_req, m_open_req.result, nullptr);
    if (status != 0) {
        std::cout << "File::close status: " << uv_strerror(status) << std::endl;
    }

    m_open_callback = nullptr;
    m_read_callback = nullptr;
    m_end_read_callback = nullptr;
}

void File::open(const std::string& path, OpenCallback callback) {
    // TODO: able to handle double open correctly (need to close previous file)
    m_path.clear();
    m_open_callback = callback;
    m_open_req.data = this;
    uv_fs_open(m_loop, &m_open_req, path.c_str(), UV_FS_O_RDWR, 0, File::on_open);
}

void File::read(ReadCallback read_callback, EndReadCallback end_read_callback) {
    m_read_callback = read_callback;
    m_end_read_callback = end_read_callback;
    m_done_read = false;
    //m_read_req.data = this;
    //m_read_state = ReadState::CONTINUE;

    schedule_read();
}

void File::read(ReadCallback callback) {
    read(callback, nullptr);
}

const std::string& File::path() const {
    return m_path;
}

void File::schedule_read() {
    //assert(m_used_read_bufs <= READ_BUFS_NUM);

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
        return;
    }

    std::cout << "using buffer with index: " << i << std::endl;

    ReadReq& read_req = m_read_reqs[i];
    read_req.is_free = false;
    read_req.data = this;

    schedule_read(read_req);
}

void File::schedule_read(ReadReq& req) {
    m_read_in_progress = true;

    if (req.raw_buf == nullptr) {
        req.raw_buf = new char[READ_BUF_SIZE];
    }

    // TODO: comments on this shared pointer
    req.buf = std::shared_ptr<char>(req.raw_buf, [this, &req](const char* p) {
        req.is_free = true;

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
    uv_fs_read(m_loop, &req, m_open_req.result, &buf, 1, -1, File::on_read);
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
    auto& this_ = *reinterpret_cast<File*>(req->data);

    if (req->result < 0) {
        // TODO: error handling
        fprintf(stderr, "Open error: %s\n", uv_strerror(req->result));
        std::cout << "'" << req->path << "'" << std::endl;
        return;
    }

    this_.m_path = req->path;
    if (this_.m_open_callback) {
        this_.m_open_callback(this_);
    }
}

void File::on_read(uv_fs_t* uv_req) {
    auto& req = *reinterpret_cast<ReadReq*>(uv_req);
    auto& this_ = *reinterpret_cast<File*>(req.data);

    this_.m_read_in_progress = false;

    if (req.result < 0) {
        // TODO: error handling
        std::cout << this_.path() << std::endl;
        fprintf(stderr, "Read error: %s\n",  uv_strerror(req.result));
    } else if (req.result == 0) {
        this_.m_done_read = true;
        req.buf.reset();

        if (this_.m_end_read_callback) {
            this_.m_end_read_callback(this_);
        }

    } else if (req.result > 0) {
        if (this_.m_read_callback) {
            this_.m_read_callback(this_, req.buf, req.result);
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

} // namespace io

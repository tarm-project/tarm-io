#include "File.h"

#include <cstring>

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


    for (size_t i = 0; i < READ_BUFS_NUM; ++i) {
        m_is_free[i] = true;
    }

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
}

void File::schedule_removal() {
    close();

    Disposable::schedule_removal();
}

void File::close() {
    std::cout << "File::close " << m_path << std::endl;

    //m_read_state = ReadState::DONE;

    uv_fs_t close_req;
    int code = uv_fs_close(m_loop, &close_req, m_open_req.result, nullptr);
    std::cout << uv_strerror(code) << std::endl;

    // uv_cancel(reinterpret_cast<uv_req_t*>(&m_open_req));
    // code = uv_cancel(reinterpret_cast<uv_req_t*>(&m_read_req));
    // uv_cancel(reinterpret_cast<uv_req_t*>(&m_write_req));

    std::cout << uv_strerror(code) << std::endl;

    uv_fs_req_cleanup(&m_open_req);
    //uv_fs_req_cleanup(&m_read_req);
    uv_fs_req_cleanup(&m_write_req);

    m_open_callback = nullptr;
    m_read_callback = nullptr;
    m_end_read_callback = nullptr;
}

void File::open(const std::string& path, OpenCallback callback) {
    m_path.clear();
    m_open_callback = callback;
    m_open_req.data = this;
    uv_fs_open(m_loop, &m_open_req, path.c_str(), UV_FS_O_RDWR, 0, File::on_open);
}

void File::read(ReadCallback read_callback, EndReadCallback end_read_callback) {
    m_read_callback = read_callback;
    m_end_read_callback = end_read_callback;
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
    bool found = false;
    for (; i < READ_BUFS_NUM; ++i) {
        if (m_is_free[i]) {
            found = true;
            break;
        }
    }

    if (!found) {
        return;
    }

    std::cout << "index: " << i << std::endl;

    m_is_free[i] = false;

    ReadReq& read_req = m_read_reqs[i];
    read_req.data = this;
    read_req.index = i;

    schedule_read(read_req);

    //++m_used_read_bufs;
}

void File::schedule_read(ReadReq& req) {
    m_read_in_progress = true;

    req.buf = std::shared_ptr<char>(new char[READ_BUF_SIZE], [this, &req](const char* p) {
        delete[] p;

        m_is_free[req.index] = true;
        schedule_read();
    });

    uv_buf_t buf = uv_buf_init(req.buf.get(), READ_BUF_SIZE);
    // TODO: error handling for uv_fs_read return value
    uv_fs_read(m_loop, &req, m_open_req.result, &buf, 1, -1, File::on_read);
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
        std::cout << "Closing file: " << this_.path() << std::endl;
        uv_fs_t close_req;
        // synchronous
        uv_fs_close(req.loop, &close_req, this_.m_open_req.result, nullptr);

        if (this_.m_end_read_callback) {
            this_.m_end_read_callback(this_);
        }

        //this_.m_read_state = ReadState::DONE;
    } else if (req.result > 0) {
        if (this_.m_read_callback) {
            this_.m_read_callback(this_, req.buf, req.result);
        }

        //if (this_.m_used_read_bufs < READ_BUFS_NUM) {
            this_.schedule_read();
        //}

        req.buf.reset();

        //if (this_.m_read_state == ReadState::CONTINUE) {
        //    this_.schedule_read();
        //}
    }
}

} // namespace io

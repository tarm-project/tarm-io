#include "File.h"

#include <cstring>

// TODO: remove
#include <iostream>

namespace io {

File::File(EventLoop& loop) :
    m_loop(&loop) {
    memset(&m_open_req, 0, sizeof(m_open_req));
    memset(&m_read_req, 0, sizeof(m_read_req));
    memset(&m_write_req, 0, sizeof(m_write_req));
}

File::~File() {
    uv_fs_req_cleanup(&m_open_req);
    uv_fs_req_cleanup(&m_read_req);
    uv_fs_req_cleanup(&m_write_req);
}

void File::open(const std::string& path, OpenCallback callback) {
    m_path.clear();
    m_open_callback = callback;
    m_open_req.data = this;
    uv_fs_open(m_loop, &m_open_req, path.c_str(), UV_FS_O_RDWR, 0, File::on_open);
}

void File::read(ReadCallback callback) {
    m_read_callback = callback;
    m_read_req.data = this;

    uv_buf_t buf = uv_buf_init(m_read_buf, sizeof(m_read_buf));
    // TODO: error handling for uv_fs_read return value
    uv_fs_read(m_loop, &m_read_req, m_open_req.result, &buf, 1, -1, File::on_read);
}

const std::string& File::path() const {
    return m_path;
}

// ////////////////////////////////////// static //////////////////////////////////////
void File::on_open(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<File*>(req->data);

    if (req->result < 0) {
        // TODO: error handling
        fprintf(stderr, "Open error: %s\n", uv_strerror(req->result));
        std::cout << req->path << std::endl;
        return;
    }

    this_.m_path = req->path;
    if (this_.m_open_callback) {
        this_.m_open_callback(this_);
    }
}

void File::on_read(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<File*>(req->data);

    if (req->result < 0) {
        // TODO: error handling
        std::cout << this_.path() << std::endl;
        fprintf(stderr, "Read error: %s\n",  uv_strerror(req->result));
    } else if (req->result == 0) {
        std::cout << "Closing file: " << this_.path() << std::endl;
        uv_fs_t close_req;
        // synchronous
        uv_fs_close(req->loop, &close_req, this_.m_open_req.result, nullptr);
    } else if (req->result > 0) {
        if (this_.m_read_callback) {
            this_.m_read_callback(this_, this_.m_read_buf, req->result);
        }

        // TODO: copypaste from File::read
        uv_buf_t buf = uv_buf_init(this_.m_read_buf, sizeof(m_read_buf));
        // TODO: error handling for uv_fs_read return value
        uv_fs_read(req->loop, &this_.m_read_req, this_.m_open_req.result, &buf, 1, -1, File::on_read);
    }
}

} // namespace io

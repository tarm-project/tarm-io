#include "Dir.h"

#include <cstring>

// TODO: remove
#include <iostream>

namespace io {

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

    return DirectoryEntryType::UNKNOWN;
}

}

const std::string& Dir::path() const {
    return m_path;
}

Dir::Dir(EventLoop& loop) :
    Disposable(loop),
    m_loop(&loop) {
    memset(&m_open_dir_req, 0, sizeof(m_open_dir_req));
    memset(&m_read_dir_req, 0, sizeof(m_read_dir_req));
}

Dir::~Dir() {
}

void Dir::open(const std::string& path, OpenCallback callback) {
    m_path = path;
    m_open_callback = callback;
    m_open_dir_req.data = this;
    uv_fs_opendir(m_loop, &m_open_dir_req, path.c_str(), Dir::on_open_dir);
}

void Dir::read(ReadCallback read_callback, EndReadCallback end_read_callback) {
    // TODO: check if open
    m_read_dir_req.data = this;
    m_read_callback = read_callback;
    m_end_read_callback = end_read_callback;

    uv_dir_t* dir = reinterpret_cast<uv_dir_t*>(m_open_dir_req.ptr);
    uv_fs_readdir(m_loop, &m_read_dir_req, dir, on_read_dir);
}

void Dir::close() {
    if (m_open_dir_req.data == nullptr) { // did not open // TODO: revise this for case when open occured with error
        return;
    }

    m_path.clear();

    uv_dir_t* dir = reinterpret_cast<uv_dir_t*>(m_open_dir_req.ptr);

    uv_fs_req_cleanup(&m_open_dir_req);
    uv_fs_req_cleanup(&m_read_dir_req);

    m_open_dir_req.data = nullptr;
    m_read_dir_req.data = nullptr;

    // synchronous
    uv_fs_t closedir_req;
    uv_fs_closedir(m_loop,
                   &closedir_req,
                   dir,
                   nullptr);
}

////////////////////////////////////// static //////////////////////////////////////
void Dir::on_open_dir(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<Dir*>(req->data);

    if (req->result < 0 ) {
        std::cerr << "Failed to open dir: " << req->path << std::endl;
        // TODO: error handling
        return;
    }

    uv_dir_t* dir = reinterpret_cast<uv_dir_t*>(req->ptr);
    dir->dirents = this_.m_dirents;
    dir->nentries = Dir::DIRENTS_NUMBER;

    if (this_.m_open_callback) {
        this_.m_open_callback(this_);
    }
}

void Dir::on_read_dir(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<Dir*>(req->data);

    uv_dir_t* dir = reinterpret_cast<uv_dir_t*>(req->ptr);

    if (req->result == 0) {
        if (this_.m_end_read_callback) {
            this_.m_end_read_callback(this_);
        }
    } else {
        if (this_.m_read_callback) {
            this_.m_read_callback(this_, this_.m_dirents[0].name, convert_direntry_type(this_.m_dirents[0].type));
        }

        uv_fs_readdir(req->loop, &this_.m_read_dir_req, dir, Dir::on_read_dir);
    }
}

void Dir::on_close_dir(uv_fs_t* req) {

}

void Dir::schedule_removal() {
    close();

    Disposable::schedule_removal();
}

} // namespace io

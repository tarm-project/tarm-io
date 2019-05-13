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

void File::open(const std::string& path, OpenFileCallback callback) {
    m_open_callback = callback;
    uv_fs_open(m_loop, &m_open_req, path.c_str(), UV_FS_O_RDWR, 0, File::on_open);
}

// ////////////////////////////////////// static //////////////////////////////////////
void File::on_open(uv_fs_t* req) {
    std::cout << "Opened file " << req->path << std::endl;
}

} // namespace io
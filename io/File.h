#pragma once

#include "Common.h"
#include "EventLoop.h"

#include <string>
#include <functional>

namespace io {

class File {
public:
    static constexpr std::size_t READ_BUF_SIZE = 1024;
    
    using OpenCallback = std::function<void(File&)>; // TODO: error handling for this callback
    using ReadCallback = std::function<void(File&, const char*, std::size_t)>;

    File(EventLoop& loop);
    ~File();

    void open(const std::string& path, OpenCallback callback); // TODO: pass open flags
    void read(ReadCallback callback);

    const std::string& path() const;
    
    // statics
    static void on_open(uv_fs_t *req);
    static void on_read(uv_fs_t *req);

private:
    OpenCallback m_open_callback = nullptr;
    ReadCallback m_read_callback = nullptr;

    EventLoop* m_loop;

    uv_fs_t m_open_req;
    uv_fs_t m_read_req;
    uv_fs_t m_write_req;

    // TODO: revise this. Probably allocation via heap or pool will be better
    char m_read_buf[READ_BUF_SIZE] = {0};
    
    std::string m_path;
};

} // namespace io

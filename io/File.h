#pragma once

#include "Common.h"
#include "EventLoop.h"

#include <string>
#include <functional>

namespace io {

class File {
public:
    using OpenFileCallback = std::function<void(File&)>;

    File(EventLoop& loop);
    ~File();

    void open(const std::string& path, OpenFileCallback callback);

    // statics
    static void on_open(uv_fs_t *req);

    const std::string& path() const;
private:
    OpenFileCallback m_open_callback = nullptr;

    EventLoop* m_loop;

    uv_fs_t m_open_req;
    uv_fs_t m_read_req;
    uv_fs_t m_write_req;

    std::string m_path;
};

} // namespace io
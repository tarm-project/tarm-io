#pragma once

#include "Common.h"
#include "EventLoop.h"

#include <string>
#include <functional>

namespace io {

class Dir {
public:
    using OpenCallback = std::function<void(Dir&)>; // TODO: error handling for this callback
    using ReadCallback = std::function<void(Dir&, const char*)>;
    using CloseCallback = std::function<void(Dir&)>;

    Dir(EventLoop& loop);
    ~Dir();

    void open(const std::string& path, OpenCallback callback);
    void read(ReadCallback callback);
    void close();

    const std::string& path() const;

    // statics
    static void on_open_dir(uv_fs_t* req);
    static void on_read_dir(uv_fs_t* req);
    static void on_close_dir(uv_fs_t* req);
private:
    static constexpr std::size_t DIRENTS_NUMBER = 1;

    EventLoop* m_loop;

    OpenCallback m_open_callback = nullptr;
    ReadCallback m_read_callback = nullptr;

    std::string m_path;

    uv_fs_t m_open_dir_req;
    uv_fs_t m_read_dir_req;

    uv_dirent_t m_dirents[DIRENTS_NUMBER];
};

} // namespace io

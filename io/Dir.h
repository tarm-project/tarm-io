#pragma once

#include "Common.h"
#include "EventLoop.h"
#include "DirectoryEntryType.h"
#include "Disposable.h"
#include "Status.h"

#include <string>
#include <functional>

namespace io {

class Dir : public Disposable {
public:
    using OpenCallback = std::function<void(Dir&, const Status&)>;
    using ReadCallback = std::function<void(Dir&, const char*, DirectoryEntryType)>;
    using CloseCallback = std::function<void(Dir&)>;
    using EndReadCallback = std::function<void(Dir&)>;

    Dir(EventLoop& loop);

    void open(const std::string& path, OpenCallback callback);
    bool is_open() const;
    void close();

    void read(ReadCallback read_callback, EndReadCallback end_read_callback = nullptr);

    void schedule_removal() override;

    const std::string& path() const;

    // statics
    static void on_open_dir(uv_fs_t* req);
    static void on_read_dir(uv_fs_t* req);
    static void on_close_dir(uv_fs_t* req);

protected:
    ~Dir();

private:
    static constexpr std::size_t DIRENTS_NUMBER = 1;

    uv_loop_t* m_uv_loop;

    OpenCallback m_open_callback = nullptr;
    ReadCallback m_read_callback = nullptr;
    EndReadCallback m_end_read_callback = nullptr;

    std::string m_path;

    uv_fs_t m_open_dir_req;
    uv_fs_t m_read_dir_req;
    uv_dir_t* m_uv_dir = nullptr;

    uv_dirent_t m_dirents[DIRENTS_NUMBER];
};

using TempDirCallback = std::function<void(const std::string&)>;
void make_temp_dir(EventLoop& loop, const std::string& name_template, TempDirCallback callback);
std::string make_temp_dir(const std::string& name_template);

} // namespace io

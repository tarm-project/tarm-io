#pragma once

#include "Common.h"
#include "EventLoop.h"
#include "Disposable.h"

#include <string>
#include <functional>

// TODO: move
#include <ostream>

namespace io {

// TODO: move to separate file
enum class DirectoryEntryType {
    UNKNOWN = 0,
    FILE,
    DIR,
    LINK,
    FIFO,
    SOCKET,
    CHAR,
    BLOCK
};

inline
std::ostream& operator<< (std::ostream& os, DirectoryEntryType type)
{
    switch (type) {
        case DirectoryEntryType::UNKNOWN:
            return os << "UNKNOWN";
        case DirectoryEntryType::FILE:
            return os << "FILE";
        case DirectoryEntryType::DIR:
            return os << "DIR";
        case DirectoryEntryType::LINK:
            return os << "LINK";
        case DirectoryEntryType::FIFO:
            return os << "FIFO";
        case DirectoryEntryType::SOCKET:
            return os << "SOCKET";
        case DirectoryEntryType::CHAR:
            return os << "CHAR";
        case DirectoryEntryType::BLOCK:
            return os << "BLOCK";
        // omit default case to trigger compiler warning for missing cases
    };
    return os << static_cast<std::uint64_t>(type);
}

class Dir : public Disposable {
public:
    using OpenCallback = std::function<void(Dir&)>; // TODO: error handling for this callback
    using ReadCallback = std::function<void(Dir&, const char*, DirectoryEntryType)>;
    using CloseCallback = std::function<void(Dir&)>;
    using EndReadCallback = std::function<void(Dir&)>;

    Dir(EventLoop& loop);
    ~Dir();

    void open(const std::string& path, OpenCallback callback);
    void read(ReadCallback read_callback, EndReadCallback end_read_callback = nullptr);
    void close();

    void schedule_removal() override;

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
    EndReadCallback m_end_read_callback = nullptr;

    std::string m_path;

    uv_fs_t m_open_dir_req;
    uv_fs_t m_read_dir_req;

    uv_dirent_t m_dirents[DIRENTS_NUMBER];
};

} // namespace io

#pragma once

#include "Common.h"
#include "Disposable.h"
#include "EventLoop.h"

#include <string>
#include <functional>
#include <vector>
#include <memory>

namespace io {

class File : public Disposable {
public:
    static constexpr std::size_t READ_BUF_SIZE = 1024;
    static constexpr std::size_t READ_BUFS_NUM = 2;

    using OpenCallback = std::function<void(File&)>; // TODO: error handling for this callback
    using ReadCallback = std::function<void(File&, const char*, std::size_t)>;
    using EndReadCallback = std::function<void(File&)>;

    enum class ReadState {
        CONTINUE = 0,
        PAUSE,
        DONE
    };

    File(EventLoop& loop);
    ~File();

    void open(const std::string& path, OpenCallback callback); // TODO: pass open flags
    void read(ReadCallback callback);
    void read(ReadCallback read_callback, EndReadCallback end_read_callback);

    void close();

    const std::string& path() const;

    void pause_read();
    void continue_read();
    ReadState read_state();

    void schedule_removal() override;

    // statics
    static void on_open(uv_fs_t *req);
    static void on_read(uv_fs_t *req);

private:
    OpenCallback m_open_callback = nullptr;
    ReadCallback m_read_callback = nullptr;
    EndReadCallback m_end_read_callback = nullptr;

    void schedule_read();
    char* current_read_buf();
    char* next_read_buf();

    EventLoop* m_loop;

    uv_fs_t m_open_req;
    uv_fs_t m_read_req;
    uv_fs_t m_write_req;

    // TODO: revise this. Probably allocation via heap or pool will be better
    //char [READ_BUF_SIZE] = {0};
    std::vector<std::unique_ptr<char[]>> m_read_bufs;
    size_t m_current_read_buf_idx = 0;

    ReadState m_read_state = ReadState::CONTINUE;

    std::string m_path;
};

} // namespace io

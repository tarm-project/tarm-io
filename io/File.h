#pragma once

#include "Common.h"
#include "Disposable.h"
#include "EventLoop.h"

#include <string>
#include <functional>
#include <vector>
#include <memory>

namespace io {

struct ReadReq : public uv_fs_t {
    std::shared_ptr<char> buf;
    int index = -1;
};

class File : public Disposable {
public:
    static constexpr std::size_t READ_BUF_SIZE = 1024;
    static constexpr std::size_t READ_BUFS_NUM = 4;

    using OpenCallback = std::function<void(File&)>; // TODO: error handling for this callback
    using ReadCallback = std::function<void(File&, std::shared_ptr<const char>, std::size_t)>;
    using EndReadCallback = std::function<void(File&)>;

    /*
    enum class ReadState {
        CONTINUE = 0,
        PAUSE,
        DONE
    };
    */

    File(EventLoop& loop);

    void open(const std::string& path, OpenCallback callback); // TODO: pass open flags
    void read(ReadCallback callback);
    void read(ReadCallback read_callback, EndReadCallback end_read_callback);

    void close();
    bool is_open() const;

    const std::string& path() const;

    /*
    void pause_read();
    void continue_read();
    ReadState read_state();
    */

    void schedule_removal() override;

    // statics
    static void on_open(uv_fs_t* req);
    static void on_read(uv_fs_t* req);
    static void on_close(uv_fs_t* req);

protected:
    ~File();

private:
    OpenCallback m_open_callback = nullptr;
    ReadCallback m_read_callback = nullptr;
    EndReadCallback m_end_read_callback = nullptr;

    void schedule_read();
    void schedule_read(ReadReq& req);
    /*
    char* current_read_buf();
    char* next_read_buf();
    */

    EventLoop* m_loop;

    ReadReq m_read_reqs[READ_BUFS_NUM];
    bool m_is_free[READ_BUFS_NUM];
    char* m_bufs[READ_BUFS_NUM];
    //std::size_t m_used_read_bufs = 0;
    bool m_read_in_progress = false;
    bool m_done_read = false; // TODO: make some states instead bunch of flags

    uv_fs_t m_open_req;
    //uv_fs_t m_read_req;
    uv_fs_t m_write_req;

    // TODO: revise this. Probably allocation via heap or pool will be better
    //char [READ_BUF_SIZE] = {0};
    //std::vector<std::unique_ptr<char[]>> m_read_bufs;
    //size_t m_current_read_buf_idx = 0;
    //ReadState m_read_state = ReadState::CONTINUE;

    std::string m_path;
};

} // namespace io

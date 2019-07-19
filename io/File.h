#pragma once

#include "Common.h"
#include "DataChunk.h"
#include "Disposable.h"
#include "EventLoop.h"
#include "Status.h"

#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <cstring>

namespace io {

// TODO: move
struct StatData : public uv_stat_t {
};

// TODO: move
struct ReadRequest : public uv_fs_t {
    ReadRequest() {
        memset(this, 0, sizeof(uv_fs_t));
    }

    ~ReadRequest() {
        delete[] raw_buf;
    }

    std::shared_ptr<char> buf;

    bool is_free = true;
    char* raw_buf = nullptr;
};

class File : public Disposable {
public:
    static constexpr std::size_t READ_BUF_SIZE = 1024 * 4;
    static constexpr std::size_t READ_BUFS_NUM = 4;

    using OpenCallback = std::function<void(File&, const Status&)>;
    using ReadCallback = std::function<void(File&, const io::DataChunk&, const Status&)>;
    using EndReadCallback = std::function<void(File&)>;
    using StatCallback = std::function<void(File&, const StatData&)>;

    File(EventLoop& loop);

    void open(const std::string& path, OpenCallback callback); // TODO: pass open flags
    void close();
    bool is_open() const;

    void read(ReadCallback callback);
    void read(ReadCallback read_callback, EndReadCallback end_read_callback);

    void read_block(off_t offset, std::size_t bytes_count, ReadCallback read_callback);

    const std::string& path() const;

    void stat(StatCallback callback);

    void schedule_removal() override;

    // statics
    static void on_open(uv_fs_t* req);
    static void on_read(uv_fs_t* req);
    static void on_read_block(uv_fs_t* req);
    static void on_stat(uv_fs_t* req);

protected:
    ~File();

private:
    OpenCallback m_open_callback = nullptr;
    ReadCallback m_read_callback = nullptr;
    EndReadCallback m_end_read_callback = nullptr;
    StatCallback m_stat_callback = nullptr;

    void schedule_read();
    void schedule_read(ReadRequest& req);

    bool has_read_buffers_in_use() const;

    EventLoop* m_loop;
    uv_loop_t* m_uv_loop;

    ReadRequest m_read_reqs[READ_BUFS_NUM];
    std::size_t m_current_offset = 0;

    // TODO: make some states instead bunch of flags
    bool m_read_in_progress = false;
    bool m_done_read = false;
    bool m_need_reschedule_remove = false;

    // TODO: it may be not reasonable to store by pointers here because they take to much space (>400b)
    //       also memory pool will help a lot
    uv_fs_t* m_open_request = nullptr;
    decltype(uv_fs_t::result) m_file_handle = -1;
    uv_fs_t m_stat_req;
    uv_fs_t m_write_req;

    std::string m_path;
};

} // namespace io

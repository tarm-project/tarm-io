#pragma once

#include "Common.h"
#include "DataChunk.h"
#include "Disposable.h"
#include "EventLoop.h"
#include "Status.h"
#include "UserDataHolder.h"

#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <cstring>

namespace io {

// TODO: move
struct StatData : public uv_stat_t {
};

class File : public Disposable,
             public UserDataHolder {
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

protected:
    ~File();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

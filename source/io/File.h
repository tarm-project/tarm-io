#pragma once

#include "DataChunk.h"
#include "Disposable.h"
#include "EventLoop.h"
#include "Export.h"
#include "Status.h"
#include "UserDataHolder.h"

#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <cstring>

//#include <uv.h>

namespace io {

// TODO: move and cover by static asserts
typedef struct {
  long tv_sec;
  long tv_nsec;
} timespec_t;

struct StatData /* : public uv_stat_t */ {
  std::uint64_t st_dev;
  std::uint64_t st_mode;
  std::uint64_t st_nlink;
  std::uint64_t st_uid;
  std::uint64_t st_gid;
  std::uint64_t st_rdev;
  std::uint64_t st_ino;
  std::uint64_t st_size;
  std::uint64_t st_blksize;
  std::uint64_t st_blocks;
  std::uint64_t st_flags;
  std::uint64_t st_gen;
  timespec_t st_atim;
  timespec_t st_mtim;
  timespec_t st_ctim;
  timespec_t st_birthtim;
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

    IO_DLL_PUBLIC File(EventLoop& loop);

    IO_DLL_PUBLIC void open(const std::string& path, OpenCallback callback); // TODO: pass open flags
    IO_DLL_PUBLIC void close();
    IO_DLL_PUBLIC bool is_open() const;

    IO_DLL_PUBLIC void read(ReadCallback callback);
    IO_DLL_PUBLIC void read(ReadCallback read_callback, EndReadCallback end_read_callback);

    IO_DLL_PUBLIC void read_block(off_t offset, unsigned int bytes_count, ReadCallback read_callback);

    IO_DLL_PUBLIC const std::string& path() const;

    IO_DLL_PUBLIC void stat(StatCallback callback);

    IO_DLL_PUBLIC void schedule_removal() override;

protected:
    IO_DLL_PUBLIC ~File();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

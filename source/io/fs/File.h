/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "EventLoop.h"
#include "Export.h"
#include "Path.h"
#include "Error.h"
#include "Removable.h"
#include "StatData.h"
#include "UserDataHolder.h"

#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <cstring>

namespace tarm {
namespace io {
namespace fs {

class File : public Removable,
             public UserDataHolder {
public:
    friend class ::tarm::io::Allocator;

    static constexpr std::size_t READ_BUF_SIZE = 1024 * 4;
    static constexpr std::size_t READ_BUFS_NUM = 4;

    using OpenCallback = std::function<void(File&, const Error&)>;
    using ReadCallback = std::function<void(File&, const DataChunk&, const Error&)>;
    using EndReadCallback = std::function<void(File&)>;
    using StatCallback = std::function<void(File&, const StatData&, const Error&)>;
    using CloseCallback = std::function<void(File&, const Error&)>;

    TARM_IO_FORBID_COPY(File);
    TARM_IO_FORBID_MOVE(File);

    TARM_IO_DLL_PUBLIC void open(const Path& path, const OpenCallback& callback); // TODO: pass open flags
    TARM_IO_DLL_PUBLIC bool is_open() const;
    TARM_IO_DLL_PUBLIC void close(const CloseCallback& close_callback = nullptr);

    TARM_IO_DLL_PUBLIC void read(const ReadCallback& callback);
    TARM_IO_DLL_PUBLIC void read(const ReadCallback& read_callback, const EndReadCallback& end_read_callback);
    TARM_IO_DLL_PUBLIC void read_block(off_t offset, unsigned int bytes_count, const ReadCallback& read_callback);

    TARM_IO_DLL_PUBLIC const Path& path() const;

    TARM_IO_DLL_PUBLIC void stat(const StatCallback& callback);

    TARM_IO_DLL_PUBLIC void schedule_removal() override;

protected:
    TARM_IO_DLL_PUBLIC File(AllocationContext& context);
    TARM_IO_DLL_PUBLIC ~File();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace fs
} // namespace io
} // namespace tarm

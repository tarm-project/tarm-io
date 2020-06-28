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

class File : public Removable,
             public UserDataHolder {
public:
    static constexpr std::size_t READ_BUF_SIZE = 1024 * 4;
    static constexpr std::size_t READ_BUFS_NUM = 4;

    using OpenCallback = std::function<void(File&, const Error&)>;
    using ReadCallback = std::function<void(File&, const DataChunk&, const Error&)>;
    using EndReadCallback = std::function<void(File&)>;
    using StatCallback = std::function<void(File&, const StatData&)>;

    IO_FORBID_COPY(File);
    IO_FORBID_MOVE(File);

    IO_DLL_PUBLIC File(EventLoop& loop);

    IO_DLL_PUBLIC void open(const Path& path, const OpenCallback& callback); // TODO: pass open flags
    IO_DLL_PUBLIC void close();
    IO_DLL_PUBLIC bool is_open() const;

    IO_DLL_PUBLIC void read(const ReadCallback& callback);
    IO_DLL_PUBLIC void read(const ReadCallback& read_callback, const EndReadCallback& end_read_callback);
    IO_DLL_PUBLIC void read_block(off_t offset, unsigned int bytes_count, const ReadCallback& read_callback);

    IO_DLL_PUBLIC const Path& path() const;

    IO_DLL_PUBLIC void stat(const StatCallback& callback);

    IO_DLL_PUBLIC void schedule_removal() override;

protected:
    IO_DLL_PUBLIC ~File();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
} // namespace tarm

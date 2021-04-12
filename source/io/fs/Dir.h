/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "Continuation.h"
#include "DirectoryEntryType.h"
#include "EventLoop.h"
#include "Export.h"
#include "Path.h"
#include "Error.h"
#include "Removable.h"
#include "UserDataHolder.h"

#include <functional>
#include <memory>
#include <string>

namespace tarm {
namespace io {
namespace fs {

class Dir : public Removable,
            public UserDataHolder {
public:
    friend class ::tarm::io::Allocator;

    using OpenCallback = std::function<void(Dir&, const Error&)>;
    using ListEntryCallback = std::function<void(Dir&, const std::string&, DirectoryEntryType)>;
    using ListEntryWithContinuationCallback = std::function<void(Dir&, const std::string&, DirectoryEntryType, Continuation&)>;
    using EndListCallback = std::function<void(Dir&, const Error&)>;
    using CloseCallback = std::function<void(Dir&, const Error&)>;

    TARM_IO_FORBID_COPY(Dir);
    TARM_IO_FORBID_MOVE(Dir);

    TARM_IO_DLL_PUBLIC void open(const Path& path, const OpenCallback& callback);
    TARM_IO_DLL_PUBLIC bool is_open() const;
    TARM_IO_DLL_PUBLIC void close(const CloseCallback& callback = nullptr);

    TARM_IO_DLL_PUBLIC void list(const ListEntryCallback& list_callback, const EndListCallback& end_list_callback = nullptr);
    TARM_IO_DLL_PUBLIC void list(const ListEntryWithContinuationCallback& list_callback, const EndListCallback& end_list_callback = nullptr);

    TARM_IO_DLL_PUBLIC void schedule_removal() override;

    TARM_IO_DLL_PUBLIC const Path& path() const;

protected:
    // To create objects use allocate() method of EventLoop
    TARM_IO_DLL_PUBLIC Dir(EventLoop& loop, Error& error);
    TARM_IO_DLL_PUBLIC ~Dir();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

using MakeTempDirCallback = std::function<void(const Path&, const Error&)>;
TARM_IO_DLL_PUBLIC
void make_temp_dir(EventLoop& loop, const Path& name_template, const MakeTempDirCallback& callback);

const int DIR_MODE_DEFAULT = 0777;

// Note: mode does nothing on Windows, should be 0
using MakeDirCallback = std::function<void(const Path&, const Error&)>;
TARM_IO_DLL_PUBLIC
void make_dir(EventLoop& loop, const Path& path, int mode, const MakeDirCallback& callback);

TARM_IO_DLL_PUBLIC
void make_all_dirs(EventLoop& loop, const Path& path, int mode, const MakeDirCallback& callback);

using RemoveDirCallback = std::function<void(const Error&)>;
using ProgressCallback = std::function<void(const std::string&)>;
TARM_IO_DLL_PUBLIC
void remove_dir(EventLoop& loop, const Path& path, const RemoveDirCallback& remove_callback, const ProgressCallback& progress_callback = nullptr);

} // namespace fs
} // namespace io
} // namespace tarm

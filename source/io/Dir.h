#pragma once

#include "Common.h"
#include "EventLoop.h"
#include "Export.h"
#include "DirectoryEntryType.h"
#include "Disposable.h"
#include "Status.h"
#include "UserDataHolder.h"

#include <functional>
#include <memory>
#include <string>

namespace io {

class Dir : public Disposable,
            public UserDataHolder {
public:
    using OpenCallback = std::function<void(Dir&, const Status&)>;
    using ReadCallback = std::function<void(Dir&, const char*, DirectoryEntryType)>;
    using CloseCallback = std::function<void(Dir&)>;
    using EndReadCallback = std::function<void(Dir&)>;

    IO_DLL_PUBLIC Dir(EventLoop& loop);

    IO_DLL_PUBLIC void open(const std::string& path, OpenCallback callback);
    IO_DLL_PUBLIC bool is_open() const;
    IO_DLL_PUBLIC void close();

    IO_DLL_PUBLIC void read(ReadCallback read_callback, EndReadCallback end_read_callback = nullptr);

    IO_DLL_PUBLIC void schedule_removal() override;

    IO_DLL_PUBLIC const std::string& path() const;

protected:
    IO_DLL_PUBLIC ~Dir();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// TODO: transfer mode ???
using MakeTempDirCallback = std::function<void(const std::string&, const Status&)>;
IO_DLL_PUBLIC
void make_temp_dir(EventLoop& loop, const std::string& name_template, MakeTempDirCallback callback);

// TODO: transfer mode
using MakeDirCallback = std::function<void(const Status&)>;
IO_DLL_PUBLIC
void make_dir(EventLoop& loop, const std::string& path, MakeDirCallback callback);

using RemoveDirCallback = std::function<void(const Status&)>;
using ProgressCallback = std::function<void(const std::string&)>;
IO_DLL_PUBLIC
void remove_dir(EventLoop& loop, const std::string& path, RemoveDirCallback remove_callback, ProgressCallback progress_callback = nullptr);

} // namespace io

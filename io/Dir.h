#pragma once

#include "Common.h"
#include "EventLoop.h"
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

    Dir(EventLoop& loop);

    void open(const std::string& path, OpenCallback callback);
    bool is_open() const;
    void close();

    void read(ReadCallback read_callback, EndReadCallback end_read_callback = nullptr);

    void schedule_removal() override;

    const std::string& path() const;

protected:
    ~Dir();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

using TempDirCallback = std::function<void(const std::string&, const Status&)>;
void make_temp_dir(EventLoop& loop, const std::string& name_template, TempDirCallback callback);

} // namespace io

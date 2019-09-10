#pragma once

#include "EventLoop.h"
#include "Export.h"

#include <memory>

namespace io {

class IO_DLL_PUBLIC_CLASS_UNIX_ONLY Disposable {
public:
    // TODO: macro for this
    Disposable(const Disposable&) = delete;
    Disposable& operator=(const Disposable&) = delete;

    Disposable(Disposable&&) = default;
    Disposable& operator=(Disposable&&) = default;

    IO_DLL_PUBLIC Disposable(EventLoop& loop);
    IO_DLL_PUBLIC virtual ~Disposable();

    // TODO: need explanation of approach!
    IO_DLL_PUBLIC virtual void schedule_removal();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

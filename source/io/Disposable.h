#pragma once

#include "EventLoop.h"
#include "Export.h"

// TODO: remove
#include <uv.h>

namespace io {

class IO_DLL_PUBLIC Disposable {
public:
    Disposable(EventLoop& loop);
    virtual ~Disposable();

    // TODO: experimental, need explanation of approach!
    virtual void schedule_removal();

    // statics
    static void on_removal(uv_idle_t* handle);

protected:
    EventLoop* m_loop;
};

} // namespace io

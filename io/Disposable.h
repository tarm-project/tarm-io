#pragma once

#include "EventLoop.h"

// TODO: remove
#include <uv.h>

namespace io {

class Disposable {
public:
    Disposable(EventLoop& loop);
    virtual ~Disposable();

    // TODO: experimental, need explanation of approach!
    void schedule_removal();

    // statics
    static void on_removal(uv_idle_t* handle);

private:
    EventLoop* m_loop;
};

} // namespace io
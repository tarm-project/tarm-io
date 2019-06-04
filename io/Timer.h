#pragma once

#include "Common.h"
#include "EventLoop.h"

#include <functional>

namespace io {

// TODO: inherit from Disposable??????
class Timer  {
public:
    using Callback = std::function<void(Timer&)>;

    Timer(EventLoop& loop);
    ~Timer();

    // TODO: this could be done with macros
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    Timer(Timer&&) = default;
    Timer& operator=(Timer&&) = default;

    // If timeout is zero, the callback fires on the next event loop iteration.
    // If repeat is non-zero, the callback fires first after timeout milliseconds
    // and then repeatedly after repeat milliseconds.
    void start(uint64_t timeout_ms, uint64_t repeat_ms, Callback callback);
    void stop();

    // statics
    static void on_timer(uv_timer_t* handle);
private:
    uv_timer_t m_timer;

    EventLoop* m_loop = nullptr;
    Callback m_callback = nullptr;
};

} // namespace io

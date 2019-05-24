#pragma once

#include "Common.h"
#include "EventLoop.h"

#include <functional>

namespace io {

// TODO: inherit from Disposable??????
class Timer : public uv_timer_t {
public:
    using Callback = std::function<void(Timer&)>;

    Timer(EventLoop& loop);
    ~Timer();

    // TODO: this could be done with macros
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    Timer(Timer&&) = default;
    Timer& operator=(Timer&&) = default;

    void start(Callback callback, uint64_t timeout_ms, uint64_t repeat_ms);
    void stop();

    // statics
    static void on_timer(uv_timer_t* handle);
private:
    std::reference_wrapper<EventLoop> m_loop;
    Callback m_callback = nullptr;
};

} // namespace io
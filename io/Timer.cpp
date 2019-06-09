#include "Timer.h"

// TODO: remove
#include <iostream>

namespace io {

Timer::Timer(EventLoop& loop) :
    m_loop(&loop) {
    uv_timer_init(reinterpret_cast<uv_loop_t*>(loop.raw_loop()), &m_timer);
    m_timer.data = this;
}

namespace {

void on_timer_close(uv_handle_t* handle) {

}

} // namespace

Timer::~Timer() {
    uv_timer_stop(&m_timer);
    uv_close(reinterpret_cast<uv_handle_t*>(&m_timer), on_timer_close);
}

void Timer::start(uint64_t timeout_ms, uint64_t repeat_ms, Callback callback) {
    uv_timer_start(&m_timer, Timer::on_timer, timeout_ms, repeat_ms);
    m_callback = callback;
}

void Timer::stop() {
    uv_timer_stop(&m_timer);
}

///////////////////////////// static /////////////////////////////
void Timer::on_timer(uv_timer_t* handle) {
    if (handle->data == nullptr) {
        return;
    }

    auto& this_ = *reinterpret_cast<Timer*>(handle);

    if (this_.m_callback) {
        this_.m_callback(this_);
    }
}

} // namespace io

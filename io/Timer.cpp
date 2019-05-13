#include "Timer.h"

// TODO: remove
#include <iostream>

namespace io {

Timer::Timer(EventLoop& loop) : 
    m_loop(loop) {
    uv_timer_init(&loop, this);
}

void foo(uv_handle_t* h) {
    std::cout << "foo" << std::endl;
    //uv_unref(h);
}

Timer::~Timer() {
    uv_timer_stop(this);
    //std::cout << "uv_is_closing: " << uv_is_closing(reinterpret_cast<uv_handle_t*>(this)) << std::endl;
    //uv_close(reinterpret_cast<uv_handle_t*>(this), foo);
    uv_close(reinterpret_cast<uv_handle_t*>(this), nullptr);
    //std::cout << "uv_is_closing: " << uv_is_closing(reinterpret_cast<uv_handle_t*>(this)) << std::endl;
    // std::cout << "uv_has_ref " << uv_has_ref(reinterpret_cast<uv_handle_t*>(this)) << std::endl;
    //uv_unref(reinterpret_cast<uv_handle_t*>(this));
    // std::cout << "uv_has_ref " << uv_has_ref(reinterpret_cast<uv_handle_t*>(this)) << std::endl;
}

void Timer::start(Callback callback, uint64_t timeout_ms, uint64_t repeat_ms) {
    uv_timer_start(this, Timer::on_timer, timeout_ms, repeat_ms);
    m_callback = callback;
}

void Timer::stop() {
    uv_timer_stop(this);
}

///////////////////////////// static /////////////////////////////
void Timer::on_timer(uv_timer_t* handle) {
    auto& this_ = *reinterpret_cast<Timer*>(handle);
    if (this_.m_callback) {
        this_.m_callback(this_);
    }
}

} // namespace io
/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "Timer.h"

#include "detail/Common.h"

#include <assert.h>

namespace tarm {
namespace io {

class Timer::Impl  {
public:
    Impl(EventLoop& loop, Error& error, Timer& parent);
    ~Impl();

    void start(uint64_t timeout_ms, uint64_t repeat_ms, const Callback& callback);
    void start(uint64_t timeout_ms, const Callback& callback);
    void start(const std::deque<std::uint64_t>& timeouts_ms, const Callback& callback);
    void start(const std::deque<std::uint64_t>& timeouts_ms, uint64_t repeat_ms, const Callback& callback);
    void start_impl();
    void stop();

    std::uint64_t timeout_ms() const;
    std::uint64_t repeat_ms() const;

    std::size_t callback_call_counter() const;

    std::chrono::milliseconds real_time_passed_since_last_callback() const;

protected:
    // statics
    static void on_timer(uv_timer_t* handle);

private:
    Timer* m_parent = nullptr;
    EventLoop* m_loop = nullptr;
    uv_timer_t* m_uv_timer = nullptr;
    Callback m_callback = nullptr;
    // Old callback is needed to save (move) current one and not overwrite itself
    // while being inside the call.
    Callback m_old_callback = nullptr;

    std::deque<std::uint64_t> m_timeouts_ms;
    uint64_t m_current_timeout_ms = 0;
    uint64_t m_repeat_ms = 0;

    std::size_t m_call_counter = 0;

    bool m_state_was_reset = false;

    std::uint64_t m_last_callback_time;
};

Timer::Impl::Impl(EventLoop& loop, Error& error, Timer& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_uv_timer(new uv_timer_t),
    m_last_callback_time(uv_hrtime()) {
    error = uv_timer_init(reinterpret_cast<uv_loop_t*>(loop.raw_loop()), m_uv_timer);
    m_uv_timer->data = this;
}

namespace {

void on_timer_close(uv_handle_t* handle) {
    delete reinterpret_cast<uv_timer_t*>(handle);
}

} // namespace

Timer::Impl::~Impl() {
    uv_timer_stop(m_uv_timer);
    uv_close(reinterpret_cast<uv_handle_t*>(m_uv_timer), on_timer_close);
}

void Timer::Impl::start(uint64_t timeout_ms, const Callback& callback) {
    start(std::deque<std::uint64_t>(1, timeout_ms), 0, callback);
}

void Timer::Impl::start(uint64_t timeout_ms, uint64_t repeat_ms, const Callback& callback) {
    start(std::deque<std::uint64_t>(1, timeout_ms), repeat_ms, callback);
}

void Timer::Impl::start(const std::deque<std::uint64_t>& timeouts_ms, const Callback& callback) {
    start(timeouts_ms, 0, callback);
}

void Timer::Impl::start(const std::deque<std::uint64_t>& timeouts_ms, uint64_t repeat_ms, const Callback& callback) {
    m_call_counter = 0;

    m_last_callback_time = uv_hrtime();

    if (timeouts_ms.empty()) {
        return;
    }

    m_state_was_reset = true;

    m_old_callback = std::move(m_callback);
    m_loop->schedule_callback([this](EventLoop&) {
        this->m_old_callback = nullptr;
    });

    m_timeouts_ms = timeouts_ms;
    m_repeat_ms = repeat_ms;
    m_callback = callback;

    start_impl();
}

void Timer::Impl::start_impl() {
    // libuv return value may not be checked here because callback != nullptr is checked above
    if (m_timeouts_ms.size() == 1) {
        uv_timer_start(m_uv_timer, on_timer, m_timeouts_ms.front(), m_repeat_ms);
    } else {
        uv_timer_start(m_uv_timer, on_timer, m_timeouts_ms.front(), 0);
    }

    m_current_timeout_ms = m_timeouts_ms.front();
    m_timeouts_ms.pop_front();
}

void Timer::Impl::stop() {
    uv_timer_stop(m_uv_timer);
}

std::uint64_t Timer::Impl::timeout_ms() const {
    return m_uv_timer ? m_current_timeout_ms : 0;
}

std::uint64_t Timer::Impl::repeat_ms() const {
    return m_uv_timer ? uv_timer_get_repeat(m_uv_timer) : 0;
}

std::size_t Timer::Impl::callback_call_counter() const {
    return m_call_counter;
}

std::chrono::milliseconds Timer::Impl::real_time_passed_since_last_callback() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::nanoseconds(uv_hrtime() - m_last_callback_time));
}

////////////////////////////////////////////// static //////////////////////////////////////////////
void Timer::Impl::on_timer(uv_timer_t* handle) {
    assert(handle->data);

    auto& this_ = *reinterpret_cast<Timer::Impl*>(handle->data);
    auto& parent_ = *this_.m_parent;

    this_.m_state_was_reset = false;
    if (this_.m_callback) {
        this_.m_callback(parent_);
    }

    this_.m_last_callback_time = uv_hrtime();

    if (this_.m_state_was_reset) {
        return;
    }

    ++this_.m_call_counter;

    if (!this_.m_timeouts_ms.empty()) {
        this_.start_impl();
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

Timer::Timer(EventLoop& loop, Error& error) :
    Removable(loop),
    m_impl(new Impl(loop, error, *this)) {
}

Timer::~Timer() {
}

void Timer::start(uint64_t timeout_ms, uint64_t repeat_ms, const Callback& callback) {
    return m_impl->start(timeout_ms, repeat_ms, callback);
}

void Timer::start(uint64_t timeout_ms, const Callback& callback) {
    return m_impl->start(timeout_ms, callback);
}

void Timer::start(const std::deque<std::uint64_t>& timeouts_ms, const Callback& callback) {
    return m_impl->start(timeouts_ms, callback);
}

void Timer::stop() {
    return m_impl->stop();
}

void Timer::schedule_removal() {
    stop();
    return Removable::schedule_removal();
}

std::uint64_t Timer::timeout_ms() const {
    return m_impl->timeout_ms();
}

std::uint64_t Timer::repeat_ms() const {
    return m_impl->repeat_ms();
}

std::size_t Timer::callback_call_counter() const {
    return m_impl->callback_call_counter();
}

std::chrono::milliseconds Timer::real_time_passed_since_last_callback() const {
    return m_impl->real_time_passed_since_last_callback();
}

} // namespace io
} // namespace tarm

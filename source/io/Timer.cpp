#include "Timer.h"

#include "detail/Common.h"

#include <assert.h>

namespace io {

class Timer::Impl  {
public:
    Impl(EventLoop& loop, Timer& parent);
    ~Impl();

    void start(uint64_t timeout_ms, uint64_t repeat_ms, Callback callback);
    void start(uint64_t timeout_ms, Callback callback);
    void start(const std::deque<std::uint64_t>& timeouts_ms, Callback callback);
    void start(const std::deque<std::uint64_t>& timeouts_ms, uint64_t repeat_ms, Callback callback);
    void start_impl();
    void stop();

    std::uint64_t timeout_ms() const;
    std::uint64_t repeat_ms() const;

    // statics
    static void on_timer(uv_timer_t* handle);

private:
    Timer* m_parent = nullptr;
    EventLoop* m_loop = nullptr;
    uv_timer_t* m_uv_timer = nullptr;
    Callback m_callback = nullptr;

    std::deque<std::uint64_t> m_timeouts_ms;
    uint64_t m_current_timeout_ms = 0;
    uint64_t m_repeat_ms = 0;
};

Timer::Impl::Impl(EventLoop& loop, Timer& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_uv_timer(new uv_timer_t) {
    // TODO: check return value
    uv_timer_init(reinterpret_cast<uv_loop_t*>(loop.raw_loop()), m_uv_timer);
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

void Timer::Impl::start(uint64_t timeout_ms, Callback callback) {
    start(std::deque<std::uint64_t>(1, timeout_ms), 0, callback);
}

void Timer::Impl::start(uint64_t timeout_ms, uint64_t repeat_ms, Callback callback) {
    start(std::deque<std::uint64_t>(1, timeout_ms), repeat_ms, callback);
}

void Timer::Impl::start(const std::deque<std::uint64_t>& timeouts_ms, Callback callback) {
    start(timeouts_ms, 0, callback);
}

void Timer::Impl::start(const std::deque<std::uint64_t>& timeouts_ms, uint64_t repeat_ms, Callback callback) {
    if (callback == nullptr) {
        return;
    }

    if (timeouts_ms.empty()) {
        return;
    }

    m_timeouts_ms = timeouts_ms;
    m_repeat_ms = repeat_ms;
    m_callback = callback;

    start_impl();
}

void Timer::Impl::start_impl() {
    // Return value may not be checked here because callback not nullptr is checked above
    if (m_timeouts_ms.size() == 1) {
        uv_timer_start(m_uv_timer, on_timer, m_timeouts_ms.front(), m_repeat_ms);
    } else {
        uv_timer_start(m_uv_timer, on_timer, m_timeouts_ms.front(), 0);
    }

    m_current_timeout_ms = m_timeouts_ms.front();
    m_timeouts_ms.pop_front();
}

void Timer::Impl::stop() {
    uv_timer_stop(m_uv_timer); // TODO: error handling
}

std::uint64_t Timer::Impl::timeout_ms() const {
    return m_uv_timer ? m_current_timeout_ms : 0;
}

std::uint64_t Timer::Impl::repeat_ms() const {
    return m_uv_timer ? uv_timer_get_repeat(m_uv_timer) : 0;
}

////////////////////////////////////////////// static //////////////////////////////////////////////
void Timer::Impl::on_timer(uv_timer_t* handle) {
    assert(handle->data);

    auto& this_ = *reinterpret_cast<Timer::Impl*>(handle->data);
    auto& parent_ = *this_.m_parent;

    if (this_.m_callback) {
        this_.m_callback(parent_);
    }

    if (!this_.m_timeouts_ms.empty()) {
        this_.start_impl();
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////



Timer::Timer(EventLoop& loop) :
    Removable(loop),
    m_impl(new Impl(loop, *this)) {
}

Timer::~Timer() {
}

void Timer::start(uint64_t timeout_ms, uint64_t repeat_ms, Callback callback) {
    return m_impl->start(timeout_ms, repeat_ms, callback);
}

void Timer::start(uint64_t timeout_ms, Callback callback) {
    return m_impl->start(timeout_ms, callback);
}

void Timer::start(const std::deque<std::uint64_t>& timeouts_ms, Callback callback) {
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

} // namespace io

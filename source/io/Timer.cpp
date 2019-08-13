#include "Timer.h"

#include <assert.h>

namespace io {

class IO_DLL_PRIVATE Timer::Impl  {
public:
    Impl(EventLoop& loop, Timer& parent);
    ~Impl();

    void start(uint64_t timeout_ms, uint64_t repeat_ms, Callback callback);
    void start(uint64_t timeout_ms, Callback callback); // TODO: unit test this
    void stop();

    // statics
    static void on_timer(uv_timer_t* handle);

private:
    Timer* m_parent = nullptr;
    EventLoop* m_loop = nullptr;
    uv_timer_t* m_uv_timer = nullptr;
    Callback m_callback = nullptr;
};

Timer::Impl::Impl(EventLoop& loop, Timer& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_uv_timer(new uv_timer_t) { // TODO: new may throw, use memory pool for UV types???
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
    start(timeout_ms, 0, callback);
}

void Timer::Impl::start(uint64_t timeout_ms, uint64_t repeat_ms, Callback callback) {
    uv_timer_start(m_uv_timer, on_timer, timeout_ms, repeat_ms);
    m_callback = callback;
}

void Timer::Impl::stop() {
    uv_timer_stop(m_uv_timer);
}

////////////////////////////////////////////// static //////////////////////////////////////////////
void Timer::Impl::on_timer(uv_timer_t* handle) {
    assert(handle->data);

    auto& this_ = *reinterpret_cast<Timer::Impl*>(handle->data);
    auto& parent_ = *this_.m_parent;

    if (this_.m_callback) {
        this_.m_callback(parent_);
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

Timer::Timer(EventLoop& loop) :
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

void Timer::stop() {
    return m_impl->stop();
}

} // namespace io

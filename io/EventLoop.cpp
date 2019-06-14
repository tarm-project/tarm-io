#include "EventLoop.h"
#include "global/Configuration.h"

#include <unordered_map>
#include <atomic>

// TODO: remove
#include <iostream>

namespace io {

namespace {

struct Idle : public uv_idle_t {
    std::function<void()> callback = nullptr;
    std::size_t id = 0;
};

} // namespace

class EventLoop::Impl : public uv_loop_t {
public:
    Impl(EventLoop& loop);
    ~Impl();

    Impl(const Impl& other) = delete;
    Impl& operator=(const Impl& other) = delete;

    Impl(Impl&& other) = default;
    Impl& operator=(Impl&& other) = default;

    // Async
    void add_work(WorkCallback work_callback, WorkDoneCallback work_done_callback = nullptr);

    // Warning: do not perform heavy calculations or blocking calls here
    std::size_t schedule_call_on_each_loop_cycle(EachLoopCycleCallback callback);
    void stop_call_on_each_loop_cycle(std::size_t handle);

    // TODO: explain this
    void start_dummy_idle();
    void stop_dummy_idle();

    // TODO: handle(convert) error codes????
    int run();

    // statics
    static void on_work(uv_work_t* req);
    static void on_after_work(uv_work_t* req, int status);
    static void on_idle(uv_idle_t* handle);
    static void on_each_loop_cycle_handler_close(uv_handle_t* handle);

private:
    EventLoop* m_loop;
    // TODO: handle wrap around
    std::size_t m_idle_it_counter = 0;
    std::unordered_map<size_t, std::unique_ptr<Idle>> m_each_loop_cycle_handlers;

    uv_timer_t* m_dummy_idle = nullptr;
    std::int64_t m_idle_ref_counter = 0;
};


EventLoop::Impl::Impl(EventLoop& loop) :
    m_loop(&loop) {
    uv_loop_init(this);
}

EventLoop::Impl::~Impl() {
    int status = uv_loop_close(this);
    if (status == UV_EBUSY) {
        m_loop->log(Severity::DEBUG, "Loop: returned EBUSY at close, running one more time");

        // Making the last attemt to close everytjing and shut down gracefully
        uv_run(this, UV_RUN_ONCE);
        uv_loop_close(this);
        m_loop->log(Severity::DEBUG, "Loop: done");
    }
}

namespace {

struct Work : public uv_work_t {
    EventLoop::WorkCallback work_callback;
    EventLoop::WorkDoneCallback work_done_callback;
};

} // namespace

void EventLoop::Impl::add_work(WorkCallback work_callback, WorkDoneCallback work_done_callback) {
    if (work_callback == nullptr) {
        return;
    }

    auto work = new Work;
    work->work_callback = work_callback;
    work->work_done_callback = work_done_callback;
    int status = uv_queue_work(this, work, EventLoop::Impl::on_work, EventLoop::Impl::on_after_work);
}

int EventLoop::Impl::run() {
    return uv_run(this, UV_RUN_DEFAULT);
}

std::size_t EventLoop::Impl::schedule_call_on_each_loop_cycle(EachLoopCycleCallback callback) {
    auto ptr = std::make_unique<Idle>();
    uv_idle_init(this, ptr.get());
    ptr->data = this;
    ptr->id = m_idle_it_counter;
    ptr->callback = callback;

    uv_idle_start(ptr.get(), EventLoop::Impl::on_idle);

    m_each_loop_cycle_handlers[m_idle_it_counter] = std::move(ptr);

    return m_idle_it_counter++;
}

void EventLoop::Impl::stop_call_on_each_loop_cycle(std::size_t handle) {
    auto it = m_each_loop_cycle_handlers.find(handle);
    if (it == m_each_loop_cycle_handlers.end()) {
        return;
    }

    uv_idle_stop(it->second.get());
    uv_close(reinterpret_cast<uv_handle_t*>(it->second.get()), EventLoop::Impl::on_each_loop_cycle_handler_close);
}

namespace {

void on_dummy_idle(uv_timer_t*) {
    // Do nothing
}

void on_dummy_idle_close(uv_handle_t* handle) {
    delete reinterpret_cast<uv_timer_t*>(handle);
}

} // namespace

void EventLoop::Impl::start_dummy_idle() {
    if (m_idle_ref_counter++) {
        return; // idle is already running
    }

    m_dummy_idle = new uv_timer_t;
    uv_timer_init(this, m_dummy_idle);

    m_dummy_idle->data = this;
    uv_timer_start(m_dummy_idle, on_dummy_idle, 1, 1);
}

void EventLoop::Impl::stop_dummy_idle() {
    if (--m_idle_ref_counter) {
        return;
    }

    m_dummy_idle->data = nullptr;
    uv_timer_stop(m_dummy_idle);
    uv_close(reinterpret_cast<uv_handle_t*>(m_dummy_idle), on_dummy_idle_close);
    m_dummy_idle = nullptr;
}

/////////////////////////////////////////// static ///////////////////////////////////////////
void EventLoop::Impl::on_work(uv_work_t* req) {
    auto& work = *reinterpret_cast<Work*>(req);
    if (work.work_callback) {
        work.work_callback();
    }
}

void EventLoop::Impl::on_after_work(uv_work_t* req, int status) {
    auto& work = *reinterpret_cast<Work*>(req);
    // TODO: check cancel status????
    if (work.work_done_callback) {
        work.work_done_callback();
    }

    // TODO: memory pool for Work????
    delete &work;
}

void EventLoop::Impl::on_idle(uv_idle_t* handle) {
    auto& idle = *reinterpret_cast<Idle*>(handle);

    if (handle->data == nullptr) {
        return;
    }

    if (idle.callback) {
        idle.callback();
    }
}

void EventLoop::Impl::on_each_loop_cycle_handler_close(uv_handle_t* handle) {
    auto& idle = *reinterpret_cast<Idle*>(handle);
    auto& this_ = *reinterpret_cast<EventLoop::Impl*>(handle->data);

    auto it = this_.m_each_loop_cycle_handlers.find(idle.id);
    if (it == this_.m_each_loop_cycle_handlers.end()) {
        return;
    }

    this_.m_each_loop_cycle_handlers.erase(it);
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

namespace {
// TODO: handle wrap around
std::atomic<std::size_t> m_loop_id_counter(0);

} // namespace

EventLoop::EventLoop() :
    Logger("Loop-" + std::to_string(m_loop_id_counter++)),
    m_impl(new EventLoop::Impl(*this)) {
    if (global::logger_callback()) {
        enable_log(global::logger_callback());
    }
}

EventLoop::~EventLoop() {
}

void EventLoop::add_work(WorkCallback work_callback, WorkDoneCallback work_done_callback) {
    return m_impl->add_work(work_callback, work_done_callback);
}

std::size_t EventLoop::schedule_call_on_each_loop_cycle(EachLoopCycleCallback callback) {
    return m_impl->schedule_call_on_each_loop_cycle(callback);
}

void EventLoop::stop_call_on_each_loop_cycle(std::size_t handle) {
    return m_impl->stop_call_on_each_loop_cycle(handle);
}

void EventLoop::start_dummy_idle() {
    return m_impl->start_dummy_idle();
}

void EventLoop::stop_dummy_idle() {
    return m_impl->stop_dummy_idle();
}

int EventLoop::run() {
    return m_impl->run();
}

void* EventLoop::raw_loop() {
    return m_impl.get();
}

} // namespace io

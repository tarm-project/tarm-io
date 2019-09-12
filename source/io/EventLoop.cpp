#include "EventLoop.h"

#include "Common.h"
#include "Logger.h"
#include "ScopeExitGuard.h"
#include "Error.h"
#include "global/Configuration.h"

#include <unordered_map>
#include <atomic>
#include <cstring>
#include <thread>
#include <mutex>
#include <deque>
#include <functional>
#include <type_traits>

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

    template<typename WorkCallbackType, typename WorkDoneCallbackType>
    void add_work(WorkCallbackType work_callback, WorkDoneCallbackType work_done_callback);

    void execute_on_loop_thread(AsyncCallback callback);

    // Warning: do not perform heavy calculations or blocking calls here
    std::size_t schedule_call_on_each_loop_cycle(EachLoopCycleCallback callback);
    void stop_call_on_each_loop_cycle(std::size_t handle);

    // TODO: explain this
    void start_dummy_idle();
    void stop_dummy_idle();

    // TODO: handle(convert) error codes????
    int run();

    bool is_running() const;

protected:
    void execute_pending_callbacks();

    // statics
    template<typename WorkCallbackType, typename WorkDoneCallbackType>
    static void on_work(uv_work_t* req);
    template<typename WorkCallbackType, typename WorkDoneCallbackType>
    static void on_after_work(uv_work_t* req, int status);
    static void on_idle(uv_idle_t* handle);
    static void on_each_loop_cycle_handler_close(uv_handle_t* handle);
    static void on_async(uv_async_t* handle);
    static void on_dummy_idle_tick(uv_timer_t*);
    static void on_dummy_idle_close(uv_handle_t* handle);

private:
    EventLoop* m_loop;
    // TODO: handle wrap around
    std::size_t m_idle_it_counter = 0;
    std::unordered_map<size_t, std::unique_ptr<Idle>> m_each_loop_cycle_handlers;

    uv_timer_t* m_dummy_idle = nullptr;
    std::int64_t m_dummy_idle_ref_counter = 0;

    std::unique_ptr<uv_async_t, std::function<void(uv_async_t*)>> m_async;
    std::deque<std::function<void()>> m_callbacks_queue;
    std::mutex m_callbacks_queue_mutex;

    bool m_is_running = false;
};

namespace {

void on_async_close(uv_handle_t* handle) {
    delete reinterpret_cast<uv_async_t*>(handle);
}

} // namespace

EventLoop::Impl::Impl(EventLoop& loop) :
    m_loop(&loop),
    m_async(new uv_async_t, [](uv_async_t* async) {
        uv_close(reinterpret_cast<uv_handle_t*>(async), on_async_close);
    }) {

    // This mutex was added because thread sanitizer complained about data race in epoll_create1
    // for test EventLoopTest.loop_in_thread
    static std::mutex loop_init_mutex;
    {
        std::lock_guard<std::mutex> guard(loop_init_mutex);
        uv_loop_init(this);
    }
    m_async->data = this;
    uv_async_init(this, m_async.get(), EventLoop::Impl::on_async);

    // unref is called to make loop exitable if it has no ohter running handles except this async
    uv_unref(reinterpret_cast<uv_handle_t*>(m_async.get()));
}

EventLoop::Impl::~Impl() {
    IO_LOG(m_loop, TRACE, "dummy_idle_ref_counter:", m_dummy_idle_ref_counter);

    {
        std::lock_guard<std::mutex> guard(m_callbacks_queue_mutex);
        m_async.reset();
        m_callbacks_queue.clear();
    }

    int status = uv_loop_close(this);
    if (status == UV_EBUSY) {
        IO_LOG(m_loop, DEBUG, "loop returned EBUSY at close, running one more time");

        // Making the last attemt to close everytjing and shut down gracefully
        status = uv_run(this, UV_RUN_ONCE);

        uv_loop_close(this);
        IO_LOG(m_loop, DEBUG, "Done");
    }
}

namespace {

template <typename WorkCallbackType, typename WorkDoneCallbackType>
struct Work : public uv_work_t {
    WorkCallbackType work_callback;
    WorkDoneCallbackType work_done_callback;

    void call_work_callback() {
        if (work_callback) {
            work_callback();
        }
    }

    void call_work_done_callback() {
        if (work_done_callback) {
            work_done_callback();
        }
    }
};

template <>
struct Work<EventLoop::WorkCallbackWithUserData, EventLoop::WorkDoneCallbackWithUserData> : public uv_work_t {
    EventLoop::WorkCallbackWithUserData work_callback;
    EventLoop::WorkDoneCallbackWithUserData work_done_callback;

    void* user_data = nullptr;

    void call_work_callback() {
        if (work_callback) {
            user_data = work_callback();
        }
    }

    void call_work_done_callback() {
        if (work_done_callback) {
            work_done_callback(user_data);
        }
    }
};

} // namespace

template<typename WorkCallbackType, typename WorkDoneCallbackType>
void EventLoop::Impl::add_work(WorkCallbackType work_callback, WorkDoneCallbackType work_done_callback) {
    if (work_callback == nullptr) {
        return;
    }

    auto work = new Work<WorkCallbackType, WorkDoneCallbackType>;
    work->work_callback = work_callback;
    work->work_done_callback = work_done_callback;
    Error error = uv_queue_work(this,
                                  work,
                                  on_work<WorkCallbackType, WorkDoneCallbackType>,
                                  on_after_work<WorkCallbackType, WorkDoneCallbackType>);
    if (error) {
        // TODO: error handling
    }

}

int EventLoop::Impl::run() {
    bool has_pending_callbacks = false;

    m_is_running = true;
    int run_status = 0;

    do {
        run_status = uv_run(this, UV_RUN_DEFAULT);

        // If there were pending async callbacks after the loop exit, executing one more run
        // because some new events may be scheduled right away.
        {
            std::lock_guard<std::mutex> guard(m_callbacks_queue_mutex);
            has_pending_callbacks = m_callbacks_queue.size();
        }

        execute_pending_callbacks();

        // TODO: is it possible to detect here that loop has new running handles and uv_run should be called or not again?
    } while(run_status == 0 && has_pending_callbacks);

    m_is_running = false;

    return run_status;
}

std::size_t EventLoop::Impl::schedule_call_on_each_loop_cycle(EachLoopCycleCallback callback) {
    std::unique_ptr<Idle> ptr(new Idle);
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

void EventLoop::Impl::start_dummy_idle() {
    IO_LOG(m_loop, TRACE, "ref_counter:", m_dummy_idle_ref_counter);

    if (m_dummy_idle_ref_counter++) {
        return; // idle is already running
    }

    IO_LOG(m_loop, TRACE, "starting timer");

    m_dummy_idle = new uv_timer_t;
    std::memset(m_dummy_idle, 0, sizeof(uv_timer_t));
    uv_timer_init(this, m_dummy_idle);
    m_dummy_idle->data = this;

    uv_timer_start(m_dummy_idle, on_dummy_idle_tick, 1, 1);
}

void EventLoop::Impl::stop_dummy_idle() {
    IO_LOG(m_loop, TRACE, "ref_counter:", m_dummy_idle_ref_counter);

    if (--m_dummy_idle_ref_counter) {
        return;
    }

    IO_LOG(m_loop, TRACE, "closing timer");

    m_dummy_idle->data = nullptr;
    uv_timer_stop(m_dummy_idle);
    uv_close(reinterpret_cast<uv_handle_t*>(m_dummy_idle), on_dummy_idle_close);
    m_dummy_idle = nullptr;
}

void EventLoop::Impl::execute_on_loop_thread(AsyncCallback callback) {
    {
        std::lock_guard<std::mutex> guard(m_callbacks_queue_mutex);
        m_callbacks_queue.push_back(callback);
    }

    uv_async_send(m_async.get());
}

bool EventLoop::Impl::is_running() const {
    return m_is_running;
}

void EventLoop::Impl::execute_pending_callbacks() {
    std::deque<std::function<void()>> callbacks_queue_copy;

    {
        std::lock_guard<std::mutex> guard(m_callbacks_queue_mutex);
        m_callbacks_queue.swap(callbacks_queue_copy);
    }

    for(auto& callback : callbacks_queue_copy) {
        callback();
    }
}

////////////////////////////////////////////// static //////////////////////////////////////////////
template<typename WorkCallbackType, typename WorkDoneCallbackType>
void EventLoop::Impl::on_work(uv_work_t* req) {
    auto& work = *reinterpret_cast<Work<WorkCallbackType, WorkDoneCallbackType>*>(req);
    work.call_work_callback();
}

template<typename WorkCallbackType, typename WorkDoneCallbackType>
void EventLoop::Impl::on_after_work(uv_work_t* req, int status) {
    // TODO: check cancel status????
    auto& work = *reinterpret_cast<Work<WorkCallbackType, WorkDoneCallbackType>*>(req);
    work.call_work_done_callback();

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

void EventLoop::Impl::on_async(uv_async_t* handle) {
    auto& this_ = *reinterpret_cast<EventLoop::Impl*>(handle->data);
    this_.execute_pending_callbacks();
}

void EventLoop::Impl::on_dummy_idle_tick(uv_timer_t* handle) {
    auto& this_ = *reinterpret_cast<EventLoop::Impl*>(handle->data);
    IO_LOG(this_.m_loop, TRACE, "_");
}

void EventLoop::Impl::on_dummy_idle_close(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<EventLoop::Impl*>(handle->loop);
    IO_LOG(this_.m_loop, TRACE, "_");

    delete reinterpret_cast<uv_timer_t*>(handle);
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

void EventLoop::execute_on_loop_thread(AsyncCallback callback) {
    m_impl->execute_on_loop_thread(callback);
}

void EventLoop::add_work(WorkCallback work_callback, WorkDoneCallback work_done_callback) {
    return m_impl->add_work(work_callback, work_done_callback);
}

void EventLoop::add_work(WorkCallbackWithUserData work_callback, WorkDoneCallbackWithUserData work_done_callback) {
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

bool EventLoop::is_running() const {
    return m_impl->is_running();
}

void* EventLoop::raw_loop() {
    return m_impl.get();
}

} // namespace io

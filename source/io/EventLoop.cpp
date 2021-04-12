/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "EventLoop.h"

#include "detail/Common.h"
#include "detail/LogMacros.h"
#include "CommonMacros.h"
#include "Logger.h"
#include "ScopeExitGuard.h"
#include "Error.h"
#include "global/Configuration.h"

#include <assert.h>

#include <atomic>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

// Need this check because we cast here pointers to std::size_t and return them as handles
static_assert(sizeof(std::size_t) == sizeof(void*), "");

namespace tarm {
namespace io {

const std::size_t EventLoop::INVALID_HANDLE;

namespace {

struct EnumClassHash {
    template <typename T>
    std::size_t operator()(T t) const {
        return static_cast<std::size_t>(t);
    }
};

struct SignalHandler : public uv_signal_t {
    enum Continuity {
        ONCE,
        REPEAT
    };

    EventLoop::SignalCallback callback = nullptr;
    Continuity continuity = Continuity::ONCE;
};

struct Idle : public uv_idle_t {
    std::function<void(EventLoop&)> callback = nullptr;
};

} // namespace

class EventLoop::Impl : public uv_loop_t {
public:
    Impl(EventLoop& loop);
    ~Impl();

    TARM_IO_FORBID_COPY(Impl)
    TARM_IO_FORBID_MOVE(Impl);

    template<typename WorkCallbackType, typename WorkDoneCallbackType>
    WorkHandle add_work(const WorkCallbackType& work_callback, const WorkDoneCallbackType& work_done_callback);
    Error cancel_work(WorkHandle handle);

    void execute_on_loop_thread(const WorkCallback& callback);

    // Warning: do not perform heavy calculations or blocking calls here
    std::size_t schedule_call_on_each_loop_cycle(const WorkCallback& callback);
    void stop_call_on_each_loop_cycle(std::size_t handle);

    void start_block_loop_from_exit();
    void stop_block_loop_from_exit();

    Error add_signal_handler(Signal signal, const SignalCallback& callback, SignalHandler::Continuity handler_continuity);
    void remove_signal_handler(Signal signal);

    Error init_async();
    Error run();

    bool is_running() const;

    void schedule_callback(const WorkCallback& callback);

    void finish();

protected:
    void execute_pending_callbacks();
    void close_signal_handlers();

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
    static void on_signal(uv_signal_t* handle, int signum);
    static void on_signal_close(uv_handle_t* handle);

private:
    EventLoop* m_parent;

    uv_timer_t* m_dummy_idle = nullptr;
    std::int64_t m_dummy_idle_ref_counter = 0;

    std::unique_ptr<uv_async_t, void(*)(uv_async_t*)> m_async;
    std::deque<std::function<void(EventLoop&)>> m_async_callbacks_queue;
    std::mutex m_async_callbacks_queue_mutex;

    bool m_is_running = false;
    bool m_run_called = false;

    std::size_t m_sync_callbacks_executor_handle = 0;
    std::function<void(EventLoop&)> m_sync_callbacks_executor_function;
    std::vector<std::function<void(EventLoop&)>> m_sync_callbacks_queue;
    bool m_have_active_sync_callbacks = false;

    std::unordered_map<EventLoop::Signal, SignalHandler*, EnumClassHash> m_signal_handlers;
};

namespace {

void on_async_close(uv_handle_t* handle) {
    delete reinterpret_cast<uv_async_t*>(handle);
}

void async_close(uv_async_t* async) {
    uv_close(reinterpret_cast<uv_handle_t*>(async), on_async_close);
}

} // namespace

EventLoop::Impl::Impl(EventLoop& parent) :
    m_parent(&parent),
    m_async(nullptr, async_close),
    m_sync_callbacks_executor_function([this](EventLoop&) {
        decltype(m_sync_callbacks_queue) queue_copy;
        queue_copy.swap(m_sync_callbacks_queue);

        for(auto&& v: queue_copy) {
            v(*m_parent);
        }

        if (m_sync_callbacks_queue.empty()) {
            stop_call_on_each_loop_cycle(m_sync_callbacks_executor_handle);
            m_have_active_sync_callbacks = false;
        }
    }) {

    this->data = &parent;

    // This mutex was added because thread sanitizer complained about data race in epoll_create1
    // for test EventLoopTest.loop_in_thread
    static std::mutex loop_init_mutex;
    {
        std::lock_guard<std::mutex> guard(loop_init_mutex);
        uv_loop_init(this);
    }

    const auto async_init_error = init_async();
    if (async_init_error) {
        LOG_ERROR(m_parent, "uv's async init failed: ", async_init_error.string());
    }
}

void EventLoop::Impl::finish() {
    LOG_TRACE(m_parent, "dummy_idle_ref_counter:", m_dummy_idle_ref_counter);

    close_signal_handlers();

    {
        std::lock_guard<std::mutex> guard(m_async_callbacks_queue_mutex);
        m_async.reset();
        m_async_callbacks_queue.clear();
    }

    int status = uv_loop_close(this);

    if (!m_run_called) {
        return;
    }

    m_sync_callbacks_queue.clear();

    if (status == UV_EBUSY) {
        // Making the last attemt to close everything and shut down gracefully
        status = uv_run(this, UV_RUN_ONCE);

        // TODO: a bit hackish approach to allow finish deinit of objects with 2-phase deinitialization like TlsClient
        status = uv_run(this, UV_RUN_ONCE);

        uv_loop_close(this);
        LOG_DEBUG(m_parent, "Done");
    }
}

EventLoop::Impl::~Impl() {
}

void EventLoop::Impl::schedule_callback(const WorkCallback& callback) {
    if (!m_have_active_sync_callbacks) {
        m_sync_callbacks_executor_handle = schedule_call_on_each_loop_cycle(m_sync_callbacks_executor_function);
        m_have_active_sync_callbacks = true;
    }

    m_sync_callbacks_queue.push_back(callback);
}

Error EventLoop::Impl::init_async() {
    assert(m_async.get() == nullptr);

    std::unique_ptr<uv_async_t> async(new uv_async_t);
    Error async_init_error = uv_async_init(this, async.get(), EventLoop::Impl::on_async);
    if (async_init_error) {
        return async_init_error;
    }

    m_async.reset(async.release()); // Can not use std::move() here because of custom deleter in m_async
    m_async->data = this;

    // unref is called to make loop exitable if it has no other running handles except this async
    uv_unref(reinterpret_cast<uv_handle_t*>(m_async.get()));

    return StatusCode::OK;
}

namespace {

struct WorkBase : public uv_work_t {
    std::atomic<bool> cancelled{false};
    std::atomic<bool> queued{false};
};

template <typename WorkCallbackType, typename WorkDoneCallbackType>
struct Work : WorkBase {
    EventLoop* m_event_loop = nullptr;
    WorkCallbackType work_callback;
    WorkDoneCallbackType work_done_callback;

    void call_work_callback() {
        if (work_callback) {
            work_callback(*m_event_loop);
        }
    }

    void call_work_done_callback(const Error& error) {
        if (work_done_callback) {
            work_done_callback(*m_event_loop, error);
        }
    }
};

template <>
struct Work<EventLoop::WorkCallbackWithUserData, EventLoop::WorkDoneCallbackWithUserData> : public WorkBase {
    EventLoop* m_event_loop = nullptr;
    EventLoop::WorkCallbackWithUserData work_callback;
    EventLoop::WorkDoneCallbackWithUserData work_done_callback;

    void call_work_callback() {
        if (work_callback) {
            this->data = work_callback(*m_event_loop);
        }
    }

    void call_work_done_callback(const Error& error) {
        if (work_done_callback) {
            work_done_callback(*m_event_loop, this->data, error);
        }
    }
};

} // namespace

template<typename WorkCallbackType, typename WorkDoneCallbackType>
EventLoop::WorkHandle EventLoop::Impl::add_work(const WorkCallbackType& work_callback,
                                                const WorkDoneCallbackType& work_done_callback) {
    if (work_callback == nullptr) {
        return INVALID_HANDLE;
    }

    auto work = new Work<WorkCallbackType, WorkDoneCallbackType>;
    work->m_event_loop = m_parent;
    work->work_callback = work_callback;
    work->work_done_callback = work_done_callback;

    this->schedule_callback([work, this](EventLoop& loop) {
        LOG_TRACE(&loop, "Queue work", work);
        if (work->cancelled) {
            LOG_TRACE(&loop, "Cancel work", work);
            work->call_work_done_callback(StatusCode::OPERATION_CANCELED);
            delete work;
            return;
        }

        work->queued = true;

        Error error = uv_queue_work(this,
                                    work,
                                    on_work<WorkCallbackType, WorkDoneCallbackType>,
                                    on_after_work<WorkCallbackType, WorkDoneCallbackType>);
        if (error) {
            work->call_work_done_callback(error);
            delete work;
            return;
        }
    });

    LOG_TRACE(m_parent, "Created work", work);

    return reinterpret_cast<std::size_t>(work);
}

Error EventLoop::Impl::cancel_work(WorkHandle handle) {
    auto work = reinterpret_cast<WorkBase*>(handle);
    work->cancelled = true;
    if (work->queued) {
        return uv_cancel(reinterpret_cast<uv_req_t*>(work));
    }

    return StatusCode::OK;
}

Error EventLoop::Impl::run() {
    bool has_pending_callbacks = false;

    m_run_called = true;
    m_is_running = true;
    int run_status = 0;

    do {
        run_status = uv_run(this, UV_RUN_DEFAULT);

        // If there were pending async callbacks after the loop exit, executing one more run
        // because some new events may be scheduled right away.
        {
            std::lock_guard<std::mutex> guard(m_async_callbacks_queue_mutex);
            has_pending_callbacks = m_async_callbacks_queue.size();
        }

        execute_pending_callbacks();
    } while(run_status == 0 && has_pending_callbacks);

    m_is_running = false;

    return run_status;
}

std::size_t EventLoop::Impl::schedule_call_on_each_loop_cycle(const WorkCallback& callback) {
    auto idle = std::unique_ptr<Idle>(new Idle);
    const Error init_error = uv_idle_init(this, idle.get());
    if (init_error) {
        return INVALID_HANDLE;
    }

    idle->data = this;
    idle->callback = callback;

    const Error start_error = uv_idle_start(idle.get(), EventLoop::Impl::on_idle);
    if (start_error) {
        return INVALID_HANDLE;
    }

    return reinterpret_cast<std::size_t>(idle.release());
}

void EventLoop::Impl::stop_call_on_each_loop_cycle(std::size_t handle) {
    if (handle == INVALID_HANDLE) {
        return;
    }

    auto& idle = *reinterpret_cast<Idle*>(handle);

    // uv_idle_stop always returns 0. So no need to handle errors.
    // Anyway we can not predict what kinds of errors could be returned and how to handle them.
    uv_idle_stop(&idle);
    uv_close(reinterpret_cast<uv_handle_t*>(&idle), EventLoop::Impl::on_each_loop_cycle_handler_close);
}

void EventLoop::Impl::start_block_loop_from_exit() {
    LOG_TRACE(m_parent, "ref_counter:", m_dummy_idle_ref_counter);

    if (m_dummy_idle_ref_counter++) {
        return; // idle is already running
    }

    LOG_TRACE(m_parent, "starting timer");

    m_dummy_idle = new uv_timer_t;
    std::memset(m_dummy_idle, 0, sizeof(uv_timer_t));
    uv_timer_init(this, m_dummy_idle);
    m_dummy_idle->data = this;

    uv_timer_start(m_dummy_idle, on_dummy_idle_tick, 1, 1);
}

void EventLoop::Impl::stop_block_loop_from_exit() {
    LOG_TRACE(m_parent, "ref_counter:", m_dummy_idle_ref_counter);

    if (--m_dummy_idle_ref_counter) {
        return;
    }

    LOG_TRACE(m_parent, "closing timer");

    m_dummy_idle->data = nullptr;
    uv_timer_stop(m_dummy_idle);
    uv_close(reinterpret_cast<uv_handle_t*>(m_dummy_idle), on_dummy_idle_close);
    m_dummy_idle = nullptr;
}

namespace {

int uv_signal_from_enum(EventLoop::Signal signal) {
    switch(signal) {
        case EventLoop::Signal::INT:
            return SIGINT;
        case EventLoop::Signal::HUP:
            return SIGHUP;
        case EventLoop::Signal::WINCH:
            return SIGWINCH;
        default:
            return 0;
    }
}

} // namespace

Error EventLoop::Impl::add_signal_handler(Signal signal, const SignalCallback& callback, SignalHandler::Continuity handler_continuity) {
    if (!callback) {
        return StatusCode::INVALID_ARGUMENT;
    }

    const auto sig_num = uv_signal_from_enum(signal);
    if (!sig_num) {
        return StatusCode::INVALID_ARGUMENT;;
    }

    auto& signal_handler = m_signal_handlers[signal];
    if (signal_handler == nullptr) {
        //auto signal_handler = new SignalHandler;
        signal_handler = new SignalHandler;
        signal_handler->data = this;
        const Error init_error = uv_signal_init(this, signal_handler);
        if (init_error) {
            return init_error;
        }
    }
    signal_handler->continuity = handler_continuity;
    signal_handler->callback = callback;

    const Error start_error = uv_signal_start(signal_handler, on_signal, sig_num);
    if (start_error) {
        return start_error;
    }

    return StatusCode::OK;
}

void EventLoop::Impl::remove_signal_handler(Signal signal) {
    auto it = m_signal_handlers.find(signal);
    if (it != m_signal_handlers.end() && it->second) {
        uv_signal_stop(it->second);
    }
}

void EventLoop::Impl::close_signal_handlers() {
    LOG_TRACE(m_parent, "");
    for (auto& k_v : m_signal_handlers) {
        const Error stop_error = uv_signal_stop(k_v.second);
        if (stop_error) {
            LOG_ERROR(m_parent, stop_error);
            continue;
        }
        uv_close(reinterpret_cast<uv_handle_t*>(k_v.second), EventLoop::Impl::on_signal_close);
    }
}

void EventLoop::Impl::execute_on_loop_thread(const WorkCallback& callback) {
    {
        std::lock_guard<std::mutex> guard(m_async_callbacks_queue_mutex);
        m_async_callbacks_queue.push_back(callback);
    }

    uv_async_send(m_async.get());
}

bool EventLoop::Impl::is_running() const {
    return m_is_running;
}

void EventLoop::Impl::execute_pending_callbacks() {
    std::deque<std::function<void(EventLoop&)>> callbacks_queue_copy;

    {
        std::lock_guard<decltype(m_async_callbacks_queue_mutex)> guard(m_async_callbacks_queue_mutex);
        m_async_callbacks_queue.swap(callbacks_queue_copy);
    }

    for(auto& callback : callbacks_queue_copy) {
        callback(*m_parent);
    }
}

////////////////////////////////////////////// static //////////////////////////////////////////////
template<typename WorkCallbackType, typename WorkDoneCallbackType>
void EventLoop::Impl::on_work(uv_work_t* req) {
    auto& this_ = *reinterpret_cast<EventLoop::Impl*>(req->loop->data);

    auto& work = *reinterpret_cast<Work<WorkCallbackType, WorkDoneCallbackType>*>(req);
    if (work.cancelled) {
        LOG_TRACE(this_.m_parent, "Cancel work", &work);
        this_.execute_on_loop_thread([&](EventLoop&) {
            work.call_work_done_callback(StatusCode::OPERATION_CANCELED);
        });
        return;
    }

    work.call_work_callback();
}

template<typename WorkCallbackType, typename WorkDoneCallbackType>
void EventLoop::Impl::on_after_work(uv_work_t* req, int status) {
    auto& work = *reinterpret_cast<Work<WorkCallbackType, WorkDoneCallbackType>*>(req);
    work.call_work_done_callback(Error(status));
    delete &work;
}

void EventLoop::Impl::on_idle(uv_idle_t* handle) {
    auto& idle = *reinterpret_cast<Idle*>(handle);

    if (handle->data == nullptr) {
        return;
    }

    auto& this_ = *reinterpret_cast<EventLoop::Impl*>(handle->data);

    if (idle.callback) {
        idle.callback(*this_.m_parent);
    }
}

void EventLoop::Impl::on_each_loop_cycle_handler_close(uv_handle_t* handle) {
    auto& idle = *reinterpret_cast<Idle*>(handle);
    auto& this_ = *reinterpret_cast<EventLoop::Impl*>(handle->data);

    LOG_TRACE(this_.m_parent, this_.m_parent, "");

    delete &idle;
}

void EventLoop::Impl::on_async(uv_async_t* handle) {
    auto& this_ = *reinterpret_cast<EventLoop::Impl*>(handle->data);
    this_.execute_pending_callbacks();
}

void EventLoop::Impl::on_dummy_idle_tick(uv_timer_t* handle) {
    auto& this_ = *reinterpret_cast<EventLoop::Impl*>(handle->data);
    LOG_TRACE(this_.m_parent, "");
}

void EventLoop::Impl::on_dummy_idle_close(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<EventLoop::Impl*>(handle->loop);
    LOG_TRACE(this_.m_parent, "");

    delete reinterpret_cast<uv_timer_t*>(handle);
}

void EventLoop::Impl::on_signal(uv_signal_t* handle, int /*signum*/) {
    auto& handler = *reinterpret_cast<SignalHandler*>(handle);
    auto& this_ = *reinterpret_cast<EventLoop::Impl*>(handle->data);
    LOG_TRACE(this_.m_parent, "");

    Error stop_error(StatusCode::OK);
    if (handler.continuity == SignalHandler::ONCE) {
        stop_error = uv_signal_stop(handle);
    }

    handler.callback(*this_.m_parent, stop_error);

    //uv_signal_stop(handle);
}

void EventLoop::Impl::on_signal_close(uv_handle_t* handle) {
    auto& handler = *reinterpret_cast<SignalHandler*>(handle);
    auto& this_ = *reinterpret_cast<EventLoop::Impl*>(handle->data);
    LOG_TRACE(this_.m_parent, "");
    delete &handler;
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

namespace {

std::atomic<std::uint64_t> m_loop_id_counter(0);

} // namespace

EventLoop::EventLoop() :
    Allocator(*this),
    Logger("Loop-" + std::to_string(m_loop_id_counter++)),
    m_impl(new EventLoop::Impl(*this)) {
    if (global::logger_callback()) {
        enable_log(global::logger_callback());
    }
}

EventLoop::~EventLoop() {
    m_impl->finish();
}

void EventLoop::execute_on_loop_thread(const WorkCallback& callback) {
    m_impl->execute_on_loop_thread(callback);
}

EventLoop::WorkHandle EventLoop::add_work(const WorkCallback& thread_pool_work_callback,
                                          const WorkDoneCallback& loop_thread_work_done_callback) {
    return m_impl->add_work(thread_pool_work_callback, loop_thread_work_done_callback);
}

EventLoop::WorkHandle EventLoop::add_work(const WorkCallbackWithUserData& thread_pool_work_callback,
                                          const WorkDoneCallbackWithUserData& loop_thread_work_done_callback) {
    return m_impl->add_work(thread_pool_work_callback, loop_thread_work_done_callback);
}

Error EventLoop::cancel_work(WorkHandle handle) {
    return m_impl->cancel_work(handle);
}

std::size_t EventLoop::schedule_call_on_each_loop_cycle(const WorkCallback& callback) {
    return m_impl->schedule_call_on_each_loop_cycle(callback);
}

void EventLoop::stop_call_on_each_loop_cycle(std::size_t handle) {
    return m_impl->stop_call_on_each_loop_cycle(handle);
}

void EventLoop::start_block_loop_from_exit() {
    return m_impl->start_block_loop_from_exit();
}

void EventLoop::stop_block_loop_from_exit() {
    return m_impl->stop_block_loop_from_exit();
}

Error EventLoop::run() {
    return m_impl->run();
}

bool EventLoop::is_running() const {
    return m_impl->is_running();
}

void* EventLoop::raw_loop() {
    return m_impl.get();
}

void EventLoop::schedule_callback(const WorkCallback& callback) {
    return m_impl->schedule_callback(callback);
}

Error EventLoop::add_signal_handler(Signal signal, const SignalCallback& callback) {
    return m_impl->add_signal_handler(signal, callback, SignalHandler::REPEAT);
}

Error EventLoop::handle_signal_once(Signal signal, const SignalCallback& callback) {
    return m_impl->add_signal_handler(signal, callback, SignalHandler::ONCE);
}

void EventLoop::remove_signal_handler(Signal signal) {
    return m_impl->remove_signal_handler(signal);
}

} // namespace io
} // namespace tarm

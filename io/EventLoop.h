#pragma once

#include "Common.h"
#include "Logger.h"

#include <functional>
#include <memory>

namespace io {

class EventLoop : public Logger {
public:
    using AsyncCallback = std::function<void()>;
    using WorkCallback = std::function<void()>;
    using WorkDoneCallback = std::function<void()>;
    using EachLoopCycleCallback = std::function<void()>;

    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop& other) = delete;
    EventLoop& operator=(const EventLoop& other) = delete;

    EventLoop(EventLoop&& other) = default;
    EventLoop& operator=(EventLoop&& other) = default;

    // Executed on thread pool
    void add_work(WorkCallback work_callback, WorkDoneCallback work_done_callback = nullptr);

    // Call callback on the EventLoop's thread. Could be executed from any thread
    // Note: this method is thread safe
    void execute_on_loop_thread(AsyncCallback callback);

    // Warning: do not perform heavy calculations or blocking calls here
    std::size_t schedule_call_on_each_loop_cycle(EachLoopCycleCallback callback);
    void stop_call_on_each_loop_cycle(std::size_t handle);

    // TODO: explain this
    void start_dummy_idle();
    void stop_dummy_idle();

    // TODO: handle(convert) error codes????
    int run();

    // TODO: make private
    void* raw_loop();
private:


    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

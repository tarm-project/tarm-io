#pragma once

#include "Export.h"
#include "Logger.h"
#include "UserDataHolder.h"

#include <functional>
#include <memory>

namespace io {

class EventLoop : public Logger,
                  public UserDataHolder {
public:
    using AsyncCallback = std::function<void()>;
    using EachLoopCycleCallback = std::function<void()>;

    using WorkCallback = std::function<void()>;
    using WorkDoneCallback = std::function<void()>;
    using WorkCallbackWithUserData = std::function<void*()>;
    using WorkDoneCallbackWithUserData = std::function<void(void*)>;

    IO_DLL_PUBLIC EventLoop();
    IO_DLL_PUBLIC ~EventLoop();

    EventLoop(const EventLoop& other) = delete;
    EventLoop& operator=(const EventLoop& other) = delete;

    EventLoop(EventLoop&& other) = default;
    EventLoop& operator=(EventLoop&& other) = default;

    // Executed on thread pool
    IO_DLL_PUBLIC void add_work(WorkCallback work_callback, WorkDoneCallback work_done_callback = nullptr);
    IO_DLL_PUBLIC void add_work(WorkCallbackWithUserData work_callback, WorkDoneCallbackWithUserData work_done_callback = nullptr);

    // Call callback on the EventLoop's thread. Could be executed from any thread
    // Note: this method is thread safe
    IO_DLL_PUBLIC void execute_on_loop_thread(AsyncCallback callback);

    // Warning: do not perform heavy calculations or blocking calls here
    IO_DLL_PUBLIC std::size_t schedule_call_on_each_loop_cycle(EachLoopCycleCallback callback);
    IO_DLL_PUBLIC void stop_call_on_each_loop_cycle(std::size_t handle);

    // TODO: explain this
    IO_DLL_PUBLIC void start_dummy_idle();
    IO_DLL_PUBLIC void stop_dummy_idle();

    // TODO: handle(convert) error codes????
    IO_DLL_PUBLIC int run();

    IO_DLL_PUBLIC bool is_running() const;

    // TODO: make private???
    IO_DLL_PUBLIC void* raw_loop();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

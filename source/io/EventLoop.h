#pragma once

#include "CommonMacros.h"
#include "Export.h"
#include "Logger.h"
#include "UserDataHolder.h"

#include <functional>
#include <memory>
#include <limits>

// TODO: memory pool for objects allocations???

// DOC: calling some loop methods and not calling run() will result in memory leak

namespace io {

class EventLoop : public Logger,
                  public UserDataHolder {
public:
    // TODO: pass loop as reference parameter in callbacks
    using AsyncCallback = std::function<void()>; // TODO: find better name than AsyncCallback
    using EachLoopCycleCallback = std::function<void()>;

    using WorkCallback = std::function<void()>;
    using WorkDoneCallback = std::function<void()>;
    using WorkCallbackWithUserData = std::function<void*()>;
    using WorkDoneCallbackWithUserData = std::function<void(void*)>;

    static const std::size_t INVALID_HANDLE = (std::numeric_limits<std::size_t>::max)();

    IO_FORBID_COPY(EventLoop);
    IO_FORBID_MOVE(EventLoop);

    IO_DLL_PUBLIC EventLoop();
    IO_DLL_PUBLIC ~EventLoop();

    // Executed on thread pool
    IO_DLL_PUBLIC void add_work(WorkCallback work_callback, WorkDoneCallback work_done_callback = nullptr);
    IO_DLL_PUBLIC void add_work(WorkCallbackWithUserData work_callback, WorkDoneCallbackWithUserData work_done_callback = nullptr);

    // Call callback on the EventLoop's thread. Could be executed from any thread
    // Note: this method is thread safe
    IO_DLL_PUBLIC void execute_on_loop_thread(AsyncCallback callback);

    // Schedule on loop's thread, will block loop. Not thread safe.
    IO_DLL_PUBLIC void schedule_callback(WorkCallback callback);

    // Warning: do not perform heavy calculations or blocking calls here
    // TODO: replace EachLoopCycleCallback -> WorkCallback ????
    IO_DLL_PUBLIC std::size_t schedule_call_on_each_loop_cycle(EachLoopCycleCallback callback);
    IO_DLL_PUBLIC void stop_call_on_each_loop_cycle(std::size_t handle);

    // TODO: explain this, make private
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

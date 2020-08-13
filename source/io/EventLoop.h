/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "Error.h"
#include "Export.h"
#include "Logger.h"
#include "UserDataHolder.h"

#include <functional>
#include <memory>
#include <limits>

// DOC: calling some loop methods and not calling run() will result in memory leak

namespace tarm {
namespace io {

class EventLoop : public Logger,
                  public UserDataHolder {
public:
    enum class Signal {
        INT = 0,
        HUP,
        WINCH
    };

    using SignalCallback = std::function<void(EventLoop&, const Error&)>;

    using WorkCallback = std::function<void(EventLoop&)>;
    using WorkDoneCallback = std::function<void(EventLoop&, const Error&)>;
    using WorkCallbackWithUserData = std::function<void*(EventLoop&)>;
    using WorkDoneCallbackWithUserData = std::function<void(EventLoop&, void*, const Error&)>;

    using WorkHandle = std::size_t;

    TARM_IO_DLL_PUBLIC static const std::size_t INVALID_HANDLE = (std::numeric_limits<std::size_t>::max)();

    TARM_IO_FORBID_COPY(EventLoop);
    TARM_IO_FORBID_MOVE(EventLoop);

    TARM_IO_DLL_PUBLIC EventLoop();
    TARM_IO_DLL_PUBLIC ~EventLoop();

    TARM_IO_DLL_PUBLIC WorkHandle add_work(const WorkCallback& thread_pool_work_callback,
                                      const WorkDoneCallback& loop_thread_work_done_callback = nullptr);
    TARM_IO_DLL_PUBLIC WorkHandle add_work(const WorkCallbackWithUserData& thread_pool_work_callback,
                                      const WorkDoneCallbackWithUserData& loop_thread_work_done_callback = nullptr);
    // Only callbacks that are still not executed could be cancelled
    TARM_IO_DLL_PUBLIC Error cancel_work(WorkHandle handle);

    // Call callback on the EventLoop's thread. Could be executed from any thread
    // Note: this method is thread safe
    TARM_IO_DLL_PUBLIC void execute_on_loop_thread(const WorkCallback& callback);

    // Schedule on loop's thread, will block loop. Not thread safe.
    TARM_IO_DLL_PUBLIC void schedule_callback(const WorkCallback& callback);

    // Warning: do not perform heavy calculations or blocking calls here
    TARM_IO_DLL_PUBLIC std::size_t schedule_call_on_each_loop_cycle(const WorkCallback& callback);
    TARM_IO_DLL_PUBLIC void stop_call_on_each_loop_cycle(std::size_t handle);

    // As loop exits when there is nothing to do, sometimes it is needed to postpone exiting.
    // Do not forget to call 'stop_block_loop_from_exit' when you are sure that loop has enough work to do.
    TARM_IO_DLL_PUBLIC void start_block_loop_from_exit();
    TARM_IO_DLL_PUBLIC void stop_block_loop_from_exit();

    TARM_IO_DLL_PUBLIC Error handle_signal_once(Signal signal, const SignalCallback& callback);
    TARM_IO_DLL_PUBLIC Error add_signal_handler(Signal signal, const SignalCallback& callback);
    TARM_IO_DLL_PUBLIC void remove_signal_handler(Signal signal);

    TARM_IO_DLL_PUBLIC Error run();

    TARM_IO_DLL_PUBLIC bool is_running() const;

    // Access to an internal libuv's event loop via void* pointer which allows you
    // to schedule raw libuv operations using tarm-io library event loop.
    TARM_IO_DLL_PUBLIC void* raw_loop();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
} // namespace tarm

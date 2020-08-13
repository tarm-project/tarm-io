/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "EventLoop.h"
#include "Export.h"
#include "Error.h"
#include "UserDataHolder.h"
#include "Removable.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>

namespace tarm {
namespace io {

class Timer : public Removable,
              public UserDataHolder {
public:
    using Callback = std::function<void(Timer&)>;

    TARM_IO_DLL_PUBLIC Timer(EventLoop& loop);

    TARM_IO_FORBID_COPY(Timer);
    TARM_IO_FORBID_MOVE(Timer);

    // If timeout is zero, the callback fires on the next event loop iteration.
    // If repeat is non-zero, the callback fires first after timeout milliseconds
    // and then repeatedly after repeat milliseconds.
    TARM_IO_DLL_PUBLIC void start(std::uint64_t timeout_ms, uint64_t repeat_ms, const Callback& callback);

    // one shot timer
    TARM_IO_DLL_PUBLIC void start(std::uint64_t timeout_ms, const Callback& callback);

    // Multiple timeouts and then stop
    TARM_IO_DLL_PUBLIC void start(const std::deque<std::uint64_t>& timeouts_ms, const Callback& callback);

    TARM_IO_DLL_PUBLIC void stop();

    TARM_IO_DLL_PUBLIC void schedule_removal() override;

    TARM_IO_DLL_PUBLIC std::uint64_t timeout_ms() const;
    TARM_IO_DLL_PUBLIC std::uint64_t repeat_ms() const;

    TARM_IO_DLL_PUBLIC std::size_t callback_call_counter() const;

    // As timeouts of timers are not precise you could call this to get real time passed
    // This is not expected to be called outside of timer callback
    TARM_IO_DLL_PUBLIC std::chrono::milliseconds real_time_passed_since_last_callback() const;
protected:
    TARM_IO_DLL_PUBLIC ~Timer();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
} // namespace tarm

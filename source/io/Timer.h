#pragma once

#include "EventLoop.h"
#include "Export.h"
#include "UserDataHolder.h"

#include <functional>
#include <memory>

namespace io {

// TODO: timer with repeat counter. For example, repeat 5 times with interval a
// TODO: timer with intervals predefined, like void start({100, 200, 300, 100500}, callback);

// TODO: inherit from Removable??????
class Timer : public UserDataHolder {
public:
    using Callback = std::function<void(Timer&)>;

    IO_DLL_PUBLIC Timer(EventLoop& loop);
    IO_DLL_PUBLIC ~Timer();

    // TODO: this could be done with macros
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    Timer(Timer&&) = default;
    Timer& operator=(Timer&&) = default;

    // If timeout is zero, the callback fires on the next event loop iteration.
    // If repeat is non-zero, the callback fires first after timeout milliseconds
    // and then repeatedly after repeat milliseconds.
    IO_DLL_PUBLIC void start(uint64_t timeout_ms, uint64_t repeat_ms, Callback callback);

    IO_DLL_PUBLIC void start(uint64_t timeout_ms, Callback callback); // TODO: unit test this

    IO_DLL_PUBLIC void stop();

private:
    class Impl;
    std::unique_ptr<Impl, std::function<void(Impl*)>> m_impl;
};

} // namespace io

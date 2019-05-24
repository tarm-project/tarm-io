#pragma once

#include "Common.h"

#include <functional>

namespace io {

class EventLoop : public uv_loop_t {
public:
    using WorkCallback = std::function<void()>;
    using WorkDoneCallback = std::function<void()>;

    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop& other) = delete;
    EventLoop& operator=(const EventLoop& other) = delete;

    EventLoop(EventLoop&& other) = default;
    EventLoop& operator=(EventLoop&& other) = default;

    void add_work(WorkCallback work_callback, WorkDoneCallback work_done_callback = nullptr);

    // TODO: handle(convert) error codes????
    int run();

    // statics
    static void on_work(uv_work_t* req);
    static void on_after_work(uv_work_t* req, int status);
};

} // namespace io

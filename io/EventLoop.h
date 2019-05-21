#pragma once

#include "Common.h"

namespace io {

class EventLoop : public uv_loop_t {
public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop& other) = delete;
    EventLoop& operator=(const EventLoop& other) = delete;

    EventLoop(EventLoop&& other) = default;
    EventLoop& operator=(EventLoop&& other) = default;

    // TODO: handle(convert) error codes????
    int run();
};

} // namespace io
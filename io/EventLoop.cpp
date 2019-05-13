#include "EventLoop.h"

// TODO: remove
#include <iostream>

namespace io {

EventLoop::EventLoop() {
    uv_loop_init(this);
}

EventLoop::~EventLoop() {
    int status = uv_loop_close(this);
    if (status == UV_EBUSY) {
        // Making the last attemt to close everytjing and shut down gracefully
        //std::cout << "status " << uv_err_name(status) << " " << uv_strerror(status) << std::endl; 
        uv_run(this, UV_RUN_ONCE);
        /*status = */uv_loop_close(this);
        //std::cout << "status " << uv_strerror(status) << std::endl; 
    }
}

int EventLoop::run() {
    return uv_run(this, UV_RUN_DEFAULT);
}

} // namespace io
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

namespace {

struct Work : public uv_work_t {
    EventLoop::WorkCallback work_callback;
    EventLoop::WorkDoneCallback work_done_callback;
};

} // namespace

void EventLoop::add_work(WorkCallback work_callback, WorkDoneCallback work_done_callback) {
    if (work_callback == nullptr) {
        return;
    }

    auto work = new Work;
    work->work_callback = work_callback;
    work->work_done_callback = work_done_callback;
    int status = uv_queue_work(this, work, EventLoop::on_work, EventLoop::on_after_work);
}

int EventLoop::run() {
    return uv_run(this, UV_RUN_DEFAULT);
}

std::size_t EventLoop::schedule_call_on_each_loop_cycle(EachLoopCycleCallback callback) {
    auto ptr = std::make_unique<Idle>();
    uv_idle_init(this, ptr.get());
    ptr->data = this;
    ptr->callback = callback;

    uv_idle_start(ptr.get(), EventLoop::on_idle);

    m_each_loop_cycle_handlers[m_idle_it_counter] = std::move(ptr);

    return m_idle_it_counter++;
}

void EventLoop::stop_call_on_each_loop_cycle(std::size_t handle) {
    auto it = m_each_loop_cycle_handlers.find(handle);
    if (it == m_each_loop_cycle_handlers.end()) {
        return;
    }

    uv_idle_stop(it->second.get());
    m_each_loop_cycle_handlers.erase(it);
}

////////////////////////////////////// static //////////////////////////////////////
void EventLoop::on_work(uv_work_t* req) {
    auto& work = *reinterpret_cast<Work*>(req);
    if (work.work_callback) {
        work.work_callback();
    }
}

void EventLoop::on_after_work(uv_work_t* req, int status) {
    auto& work = *reinterpret_cast<Work*>(req);
    // TODO: check cancel status????
    if (work.work_done_callback) {
        work.work_done_callback();
    }

    // TODO: memory pool for Work????
    delete &work;
}

void EventLoop::on_idle(uv_idle_t* handle) {
    auto& idle = *reinterpret_cast<Idle*>(handle);

    if (handle->data == nullptr) {
        return;
    }

    if (idle.callback) {
        idle.callback();
    }
}

} // namespace io

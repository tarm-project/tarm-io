#include "Disposable.h"

// TODO: remove
#include <iostream>

namespace io {

namespace {

void on_delete_idle_handle_close(uv_handle_t* handle) {
    delete reinterpret_cast<uv_idle_t*>(handle);
}

} // namespace

Disposable::Disposable(EventLoop& loop) :
    m_loop(&loop) {
}

Disposable::~Disposable() {
}

void Disposable::schedule_removal() {
    if (!m_loop->is_running()) {
        m_loop->log(Logger::Severity::ERROR, "Scheduling removal after the loop finished run. This may lead to memory leaks or memory corruption.");
    }

    auto idle_ptr = new uv_idle_t;
    idle_ptr->data = this;
    uv_idle_init(reinterpret_cast<uv_loop_t*>(m_loop->raw_loop()), idle_ptr); // TODO: error handling
    uv_idle_start(idle_ptr, Disposable::on_removal);
}

void Disposable::on_removal(uv_idle_t* handle) {
    uv_idle_stop(handle);
    uv_close(reinterpret_cast<uv_handle_t*>(handle), on_delete_idle_handle_close);
    delete reinterpret_cast<Disposable*>(handle->data);
}

} // namespace io

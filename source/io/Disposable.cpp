#include "Disposable.h"

#include "Common.h"
#include "Error.h"

namespace io {

class Disposable::Impl {
public:
    Impl(EventLoop& loop, Disposable& parent);
    ~Impl();

    void schedule_removal();

    void set_on_schedule_removal(OnScheduleRemovalCallback callback);

protected:
    // statics
    static void on_removal(uv_idle_t* handle);

private:
    EventLoop* m_loop;
    Disposable* m_parent;

    OnScheduleRemovalCallback m_on_remove_callback = nullptr;
};

namespace {

void on_delete_idle_handle_close(uv_handle_t* handle) {
    delete reinterpret_cast<Disposable*>(handle->data);
    delete reinterpret_cast<uv_idle_t*>(handle);
}

} // namespace

Disposable::Impl::Impl(EventLoop& loop, Disposable& parent) :
    m_loop(&loop),
    m_parent(&parent) {
}

Disposable::Impl::~Impl() {
}

void Disposable::Impl::schedule_removal() {
    if (!m_loop->is_running()) {
        IO_LOG(m_loop, ERROR, "Scheduling removal after the loop finished run. This may lead to memory leaks or memory corruption.");
    }

    if (m_on_remove_callback) {
        m_on_remove_callback(*m_parent);
    }

    auto idle_ptr = new uv_idle_t;
    idle_ptr->data = m_parent;

    // TODO: error handling
    Error init_error = uv_idle_init(reinterpret_cast<uv_loop_t*>(m_loop->raw_loop()), idle_ptr);
    Error start_error = uv_idle_start(idle_ptr, on_removal);
}

void Disposable::Impl::set_on_schedule_removal(OnScheduleRemovalCallback callback) {
    m_on_remove_callback = callback;
}

////////////////////////////////////////////// static //////////////////////////////////////////////

void Disposable::Impl::on_removal(uv_idle_t* handle) {
    uv_idle_stop(handle);
    uv_close(reinterpret_cast<uv_handle_t*>(handle), on_delete_idle_handle_close);
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

Disposable::Disposable(EventLoop& loop) :
    m_impl(new Impl(loop, *this)) {
}

Disposable::~Disposable() {
}

void Disposable::schedule_removal() {
    return m_impl->schedule_removal();
}

void Disposable::set_on_schedule_removal(OnScheduleRemovalCallback callback) {
    return m_impl->set_on_schedule_removal(callback);
}

} // namespace io

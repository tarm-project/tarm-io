#include "Removable.h"

#include "Common.h"
#include "Error.h"

namespace io {

class Removable::Impl {
public:
    Impl(EventLoop& loop, Removable& parent);
    ~Impl();

    void schedule_removal();

    void set_on_schedule_removal(OnScheduleRemovalCallback callback);

protected:
    // statics
    static void on_removal(uv_idle_t* handle);

private:
    EventLoop* m_loop;
    Removable* m_parent;

    OnScheduleRemovalCallback m_on_remove_callback = nullptr;
};

namespace {

void on_delete_idle_handle_close(uv_handle_t* handle) {
    delete reinterpret_cast<Removable*>(handle->data);
    delete reinterpret_cast<uv_idle_t*>(handle);
}

} // namespace

Removable::Impl::Impl(EventLoop& loop, Removable& parent) :
    m_loop(&loop),
    m_parent(&parent) {
}

Removable::Impl::~Impl() {
}

void Removable::Impl::schedule_removal() {
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

void Removable::Impl::set_on_schedule_removal(OnScheduleRemovalCallback callback) {
    m_on_remove_callback = callback;
}

////////////////////////////////////////////// static //////////////////////////////////////////////

void Removable::Impl::on_removal(uv_idle_t* handle) {
    uv_idle_stop(handle);
    uv_close(reinterpret_cast<uv_handle_t*>(handle), on_delete_idle_handle_close);
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

IO_DEFINE_DEFAULT_MOVE(Removable);

Removable::Removable(EventLoop& loop) :
    m_impl(new Impl(loop, *this)) {
}

Removable::~Removable() {
}

void Removable::schedule_removal() {
    return m_impl->schedule_removal();
}

void Removable::set_on_schedule_removal(OnScheduleRemovalCallback callback) {
    return m_impl->set_on_schedule_removal(callback);
}

} // namespace io

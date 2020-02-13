#include "Removable.h"

#include "detail/Common.h"
#include "Error.h"

namespace io {

class Removable::Impl {
public:
    Impl(EventLoop& loop, Removable& parent);
    ~Impl();

    void schedule_removal();

    void set_on_schedule_removal(OnScheduleRemovalCallback callback);

    bool is_removal_scheduled() const;

    static DefaultDelete default_delete();

protected:
    // statics
    static void on_removal(uv_idle_t* handle);
    static void on_delete_idle_handle_close(uv_handle_t* handle);

private:
    EventLoop* m_loop;
    Removable* m_parent;

    OnScheduleRemovalCallback m_on_remove_callback = nullptr;

    bool m_removal_scheduled = false;
    bool m_about_to_remove = false;

    static Removable::DefaultDelete m_default_deleter;
};

Removable::DefaultDelete Removable::Impl::m_default_deleter = [](Removable* r) {
    r->schedule_removal();
};

Removable::Impl::Impl(EventLoop& loop, Removable& parent) :
    m_loop(&loop),
    m_parent(&parent) {
}

Removable::Impl::~Impl() {
}

bool Removable::Impl::is_removal_scheduled() const {
    return m_removal_scheduled;
}

void Removable::Impl::schedule_removal() {
    m_removal_scheduled = true;

    if (!m_loop->is_running()) {
        IO_LOG(m_loop, ERROR, "Scheduling removal after the loop finished run. This may lead to memory leaks or memory corruption.");
    }

    auto idle_ptr = new uv_idle_t;
    idle_ptr->data = this;

    // TODO: error handling
    Error init_error = uv_idle_init(reinterpret_cast<uv_loop_t*>(m_loop->raw_loop()), idle_ptr);
    Error start_error = uv_idle_start(idle_ptr, on_removal);
}

void Removable::Impl::set_on_schedule_removal(OnScheduleRemovalCallback callback) {
    if (m_about_to_remove) {
        return;
    }

    m_on_remove_callback = callback;
}

////////////////////////////////////////////// static //////////////////////////////////////////////

void Removable::Impl::on_delete_idle_handle_close(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<Removable::Impl*>(handle->data);

    delete this_.m_parent;
    delete reinterpret_cast<uv_idle_t*>(handle);
}

void Removable::Impl::on_removal(uv_idle_t* handle) {
    auto& this_ = *reinterpret_cast<Removable::Impl*>(handle->data);

    this_.m_about_to_remove = true;

    if (this_.m_on_remove_callback) {
        this_.m_on_remove_callback(*this_.m_parent);
    }

    uv_idle_stop(handle);
    uv_close(reinterpret_cast<uv_handle_t*>(handle), on_delete_idle_handle_close);
}

Removable::DefaultDelete Removable::Impl::default_delete() {
    return m_default_deleter;
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

bool Removable::is_removal_scheduled() const {
    return m_impl->is_removal_scheduled();
}

Removable::DefaultDelete Removable::default_delete() {
    return Impl::default_delete();
}

} // namespace io

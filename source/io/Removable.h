#pragma once

#include "EventLoop.h"
#include "Export.h"

#include <memory>
#include <functional>

namespace io {

class IO_DLL_PUBLIC_CLASS_UNIX_ONLY Removable {
public:
    friend class RefCounted;

    using OnScheduleRemovalCallback = std::function<void(const Removable&)>;

    // TODO: macro for this
    Removable(const Removable&) = delete;
    Removable& operator=(const Removable&) = delete;

    Removable(Removable&&) = default;
    Removable& operator=(Removable&&) = default;

    IO_DLL_PUBLIC Removable(EventLoop& loop);
    IO_DLL_PUBLIC virtual ~Removable();

    IO_DLL_PUBLIC virtual void schedule_removal();

    // TODO: test this
    // TODO: test calling this function inside OnScheduleRemovalCallback callback???
    IO_DLL_PUBLIC void set_on_schedule_removal(OnScheduleRemovalCallback callback);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// TODO: move
class IO_DLL_PUBLIC RefCounted {
public:
    RefCounted(Removable& Removable) :
        m_Removable(&Removable),
        m_ref_counter(0) {
    }

    void ref() {
        ++m_ref_counter;
    }

    void unref() {
        if (!m_ref_counter) {
            return; // TODO: review this
        }

        --m_ref_counter;

        if (m_ref_counter == 0) {
            m_Removable->schedule_removal();
        }
    }

private:
    Removable* m_Removable;
    std::size_t m_ref_counter;
};

} // namespace io

#pragma once

#include "EventLoop.h"
#include "Export.h"

#include <memory>
#include <functional>

namespace io {

class IO_DLL_PUBLIC_CLASS_UNIX_ONLY Disposable {
public:
    friend class RefCounted;

    using OnScheduleRemovalCallback = std::function<void(const Disposable&)>;

    // TODO: macro for this
    Disposable(const Disposable&) = delete;
    Disposable& operator=(const Disposable&) = delete;

    Disposable(Disposable&&) = default;
    Disposable& operator=(Disposable&&) = default;

    IO_DLL_PUBLIC Disposable(EventLoop& loop);
    IO_DLL_PUBLIC virtual ~Disposable();

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
    RefCounted(Disposable& disposable) :
        m_disposable(&disposable),
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
            m_disposable->schedule_removal();
        }
    }

private:
    Disposable* m_disposable;
    std::size_t m_ref_counter;
};

} // namespace io

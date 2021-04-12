#include "Allocator.h"

#include "Timer.h"

namespace tarm {
namespace io {

class Allocator::Impl {
public:
    Impl(EventLoop& loop);
    ~Impl();

    TARM_IO_FORBID_COPY(Impl)
    TARM_IO_FORBID_MOVE(Impl);

    template<typename T>
    T* allocate() {
        Error error;
        auto ptr = new(std::nothrow) T(*m_loop, error);
        m_last_allocation_error = error;
        if (error) {
            return nullptr;
        }

        if (ptr == nullptr) {
            m_last_allocation_error = StatusCode::OUT_OF_MEMORY;
            return nullptr;
        }

        return ptr;
    }

    Error last_allocation_error() const;

private:
    EventLoop* m_loop;
    static thread_local Error m_last_allocation_error;
};

thread_local Error Allocator::Impl::m_last_allocation_error{StatusCode::OK};

Allocator::Impl::Impl(EventLoop& loop) :
    m_loop(&loop) {
}

Allocator::Impl::~Impl() {
}

Error Allocator::Impl::last_allocation_error() const {
    return m_last_allocation_error;
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

Allocator::Allocator(EventLoop& loop) :
    m_impl(new Allocator::Impl(loop)) {

}

Allocator::~Allocator() = default;

template<typename T>
T* Allocator::allocate() {
    return m_impl->allocate<T>();
}

Error Allocator::last_allocation_error() const {
    return m_impl->last_allocation_error();
}

template TARM_IO_DLL_PUBLIC Timer* Allocator::allocate<Timer>();

} // namespace io
} // namespace tarm

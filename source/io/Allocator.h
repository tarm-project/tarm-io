#pragma once

#include "Error.h"
#include "Export.h"
#include "Forward.h"

#include <memory>
#include <utility>

namespace tarm {
namespace io {

class Allocator {
public:
    Allocator(EventLoop& loop) :
        m_loop(&loop) {
    }

    template<typename T, typename... ARGS>
    T* allocate(ARGS&&... args) {
        Error error;
        auto ptr = new(std::nothrow) T(*m_loop, error, std::forward<ARGS>(args)...);
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

    TARM_IO_DLL_PUBLIC Error last_allocation_error() const {
        return m_last_allocation_error;
    }

private:
    EventLoop* m_loop;
    // TODO: remove thread_local?
    static thread_local Error m_last_allocation_error;


/*
    template<typename T>
    TARM_IO_DLL_PUBLIC T* allocate();

    template<typename T, typename... ARGS>
    TARM_IO_DLL_PUBLIC T* allocate(ARGS&&... args);

    TARM_IO_DLL_PUBLIC Error last_allocation_error() const;

protected:
    Allocator(EventLoop& loop);
    ~Allocator();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
*/
};


} // namespace io
} // namespace tarm

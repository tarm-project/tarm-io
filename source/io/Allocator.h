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
    // thread safe
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
};

} // namespace io
} // namespace tarm

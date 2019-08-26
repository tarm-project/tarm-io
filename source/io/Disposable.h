#pragma once

#include "EventLoop.h"
#include "Export.h"

#include <memory>

namespace io {

// https://stackoverflow.com/questions/767579/exporting-classes-containing-std-objects-vector-map-etc-from-a-dll
// template class DLL_EXPORT std::allocator<tCharGlyphProviderRef>;

class IO_DLL_PUBLIC Disposable {
public:
    Disposable(EventLoop& loop);
    virtual ~Disposable();

    // TODO: need explanation of approach!
    virtual void schedule_removal();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

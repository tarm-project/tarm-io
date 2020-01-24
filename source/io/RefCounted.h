#pragma once

#include "CommonMacros.h"
#include "Export.h"
#include "Removable.h"

namespace io {

class RefCounted {
public:
    IO_FORBID_COPY(RefCounted);
    IO_ALLOW_MOVE(RefCounted);

    IO_DLL_PUBLIC
    RefCounted(Removable& Removable);

    IO_DLL_PUBLIC
    void ref();

    IO_DLL_PUBLIC
    void unref();

    IO_DLL_PUBLIC
    std::size_t ref_count();

private:
    Removable* m_Removable;
    std::size_t m_ref_counter;
};

} // namespace io

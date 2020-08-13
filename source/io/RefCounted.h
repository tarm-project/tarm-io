/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "Export.h"
#include "Removable.h"

namespace tarm {
namespace io {

class RefCounted : public Removable {
public:
    TARM_IO_FORBID_COPY(RefCounted);
    TARM_IO_FORBID_MOVE(RefCounted);

    TARM_IO_DLL_PUBLIC
    RefCounted(EventLoop& loop);

    TARM_IO_DLL_PUBLIC
    void ref();

    TARM_IO_DLL_PUBLIC
    void unref();

    TARM_IO_DLL_PUBLIC
    std::size_t ref_count();

private:
    std::size_t m_ref_counter;
};

} // namespace io
} // namespace tarm

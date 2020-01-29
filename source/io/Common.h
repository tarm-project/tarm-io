#pragma once

#include "detail/ConstexprString.h"

#include "Export.h"

#include <uv.h>

// TODO:
#ifdef ERROR
    #undef ERROR
#endif // ERROR

#include <cstdint>
#include <string>

// TODO: Need to remove this file

namespace io {

IO_DLL_PUBLIC
void default_alloc_buffer(uv_handle_t* /*handle*/, size_t suggested_size, uv_buf_t* buf);



} // namespace io

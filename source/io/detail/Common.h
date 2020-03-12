#pragma once

#include "ConstexprString.h"

#include <uv.h>

// Exctracted from uv-common.h which is not exported.
// This constant is used by uv_handle flags
enum {
    IO_UV_HANDLE_BOUND = 0x00002000
};

#include <cstring>

namespace io {
namespace detail {

void default_alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

} // namespace detail
} // namespace io

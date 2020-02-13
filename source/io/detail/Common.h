#pragma once

#include "ConstexprString.h"

#include <uv.h>

#include <cstring>

namespace io {
namespace detail {

void default_alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

} // namespace detail
} // namespace io

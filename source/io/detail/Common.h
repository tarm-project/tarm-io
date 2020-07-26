/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "ConstexprString.h"

#include <uv.h>

// Exctracted from uv-common.h which is not exported.
// This constant is used by uv_handle flags
enum {
    IO_UV_HANDLE_BOUND = 0x00002000,
    IO_UV_HANDLE_SHUTTING = 0x00000100
};

#include <cstring>

namespace tarm {
namespace io {
namespace detail {

void default_alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

} // namespace detail
} // namespace io
} // namespace tarm

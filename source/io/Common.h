#pragma once

#include "Export.h"

#include <uv.h>
#include <cstdint>
#include <string>

// TODO: Need to remove this file and extract conversion functions

namespace io {

IO_DLL_PUBLIC
void default_alloc_buffer(uv_handle_t* /*handle*/, size_t suggested_size, uv_buf_t* buf);

IO_DLL_PUBLIC
std::string ip4_addr_to_string(std::uint32_t addr);

} // namespace io

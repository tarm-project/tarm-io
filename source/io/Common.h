#pragma once

#include "Export.h"

#include "detail/ConstexprString.h"

#include <uv.h>

// TODO:
#ifdef ERROR
    #undef ERROR
#endif // ERROR

#include <cstdint>
#include <string>

// TODO: Need to remove this file and extract conversion functions

namespace io {

using AllocSizeType = decltype(uv_buf_t::len);

IO_DLL_PUBLIC
void default_alloc_buffer(uv_handle_t* /*handle*/, AllocSizeType suggested_size, uv_buf_t* buf);

IO_DLL_PUBLIC
std::string ip4_addr_to_string(std::uint32_t addr);

} // namespace io

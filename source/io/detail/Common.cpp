/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "Common.h"

namespace tarm {
namespace io {
namespace detail {

void default_alloc_buffer(uv_handle_t* /*handle*/, std::size_t suggested_size, uv_buf_t* buf) {
    buf->base = new char[suggested_size];
    buf->len = static_cast<decltype(uv_buf_t::len)>(suggested_size); // This cast is OK because suggested size is not more than 64 kB
}

} // namespace detail
} // namespace io
} // namespace tarm

#include "Common.h"

//#include <boost/pool/pool.hpp>

namespace io {

void default_alloc_buffer(uv_handle_t* /*handle*/, std::size_t suggested_size, uv_buf_t* buf) {
    buf->base = new char[suggested_size];
    buf->len = static_cast<decltype(uv_buf_t::len)>(suggested_size); // This cast is OK because suggested size is not more than 64 kB
}

} // namespace io

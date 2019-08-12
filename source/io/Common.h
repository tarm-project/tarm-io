#pragma once

#include <uv.h>
#include <memory>
#include <cstdint>
#include <string>

namespace io {

using TcpPtr = std::unique_ptr<uv_tcp_t>;

void default_alloc_buffer(uv_handle_t* /*handle*/, size_t suggested_size, uv_buf_t* buf);

std::string ip4_addr_to_string(std::uint32_t addr);

} // namespace io
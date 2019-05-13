#include "Common.h"

#include <boost/pool/pool.hpp>

namespace io {

void default_alloc_buffer(uv_handle_t* /*handle*/, size_t suggested_size, uv_buf_t* buf) {
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

namespace {

} // namespace

std::string ip4_addr_to_string(std::uint32_t addr) {
    std::string result;

    const std::uint8_t addr_1 = (addr & 0xFF000000u) >> 24;
    const std::uint8_t addr_2 = (addr & 0x00FF0000u) >> 16;
    const std::uint8_t addr_3 = (addr & 0x0000FF00u) >> 8;
    const std::uint8_t addr_4 = (addr & 0x000000FFu);

    // TODO: measure performance of this. May try to use result.push_back insttead of += std::to_string(...
    result.reserve(16);
    result += std::to_string(addr_1);
    result += ".";
    result += std::to_string(addr_2);
    result += ".";
    result += std::to_string(addr_3);
    result += ".";
    result += std::to_string(addr_4);

    return result;
}

} // namespace io
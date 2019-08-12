#include "Common.h"

#include <boost/pool/pool.hpp>

namespace io {

void default_alloc_buffer(uv_handle_t* /*handle*/, size_t suggested_size, uv_buf_t* buf) {
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

namespace {

void push_byte_as_str(std::string& s, std::uint8_t byte) {
    if (byte >= 100) {
        s.push_back(byte / 100 + '0');
        s.push_back((byte % 100) / 10 + '0');
        s.push_back((byte % 10) + '0');
    } else if (byte >= 10) {
        s.push_back((byte % 100) / 10 + '0');
        s.push_back((byte % 10) + '0');
    } else {
        s.push_back((byte % 10) + '0');
    }
}

} // namespace

// This implementation works x8 times faster comparing to more sraightforward approach like
// result += std::to_string((addr & 0xFF000000u) >> 24);
// result += ".";
// ...
std::string ip4_addr_to_string(std::uint32_t addr) {
    std::string result;

    result.reserve(16);
    push_byte_as_str(result, (addr & 0xFF000000u) >> 24);
    result.push_back('.');
    push_byte_as_str(result, (addr & 0x00FF0000u) >> 16);
    result.push_back('.');
    push_byte_as_str(result, (addr & 0x0000FF00u) >> 8);
    result.push_back('.');
    push_byte_as_str(result, (addr & 0x000000FFu));

    return result;
}

} // namespace io
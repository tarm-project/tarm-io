/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "Convert.h"

#include "ByteSwap.h"

#include <uv.h>

namespace tarm {
namespace io {

// IPv4
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
// And this works about x4 times faster than uv_inet_ntop
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

Error string_to_ip4_addr(const std::string& string_address, std::uint32_t& uint_address) {
    struct sockaddr_in addr;
    const Error convert_error = uv_ip4_addr(string_address.c_str(), 0, &addr);
    if (convert_error) {
        return convert_error;
    }

    uint_address = network_to_host(addr.sin_addr.s_addr);
    return Error(0);
}

// IPv6
std::string ip6_addr_to_string(const std::uint8_t* address_bytes) {
    if (address_bytes == nullptr) {
        return "";
    }

    char buf[INET6_ADDRSTRLEN];
    // Not handling error here because this function returns error when no spece left
    uv_inet_ntop(AF_INET6, address_bytes, buf, INET6_ADDRSTRLEN);

    return buf;
}

std::string ip6_addr_to_string(std::initializer_list<std::uint8_t> address_bytes) {
    return ip6_addr_to_string(&*address_bytes.begin());
}

Error string_to_ip6_addr(const std::string& string_address, std::array<std::uint8_t, 16>& bytes) {
    const Error convert_error = uv_inet_pton(AF_INET6, string_address.c_str(), bytes.data());
    if (convert_error) {
        return convert_error;
    }

    return Error(0);
}


} // namespace io
} // namespace tarm

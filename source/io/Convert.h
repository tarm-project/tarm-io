#pragma once

#include "Error.h"
#include "Export.h"

#include <string>
#include <cstdint>

namespace io {

IO_DLL_PUBLIC
std::string ip4_addr_to_string(std::uint32_t addr);

IO_DLL_PUBLIC
Error string_to_ip4_addr(const std::string& string_address, std::uint32_t& uint_address);

} // namespace io

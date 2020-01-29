#pragma once

#include "Export.h"

#include <string>
#include <cstdint>

namespace io {

IO_DLL_PUBLIC
std::string ip4_addr_to_string(std::uint32_t addr);

IO_DLL_PUBLIC
std::uint32_t string_to_ip4_addr(const std::string& address);

} // namespace io

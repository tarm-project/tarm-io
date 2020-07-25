/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"

#include <string>

#define TARM_IO_VERSION_MAJOR 1
#define TARM_IO_VERSION_MINOR 0
#define TARM_IO_VERSION_PATCH 0

#define TARM_IO_VERSION_HEX  ((TARM_IO_VERSION_MAJOR << 16) | \
                              (TARM_IO_VERSION_MINOR <<  8) | \
                              (TARM_IO_VERSION_PATCH))

namespace tarm {
namespace io {

IO_DLL_PUBLIC
unsigned version_number(void);

IO_DLL_PUBLIC
std::string version_string(void);

} // namespace io
} // namespace tarm


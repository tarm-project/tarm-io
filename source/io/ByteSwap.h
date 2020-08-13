/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "Export.h"

namespace tarm {
namespace io {

// network_to_host
template <typename T>
T network_to_host(T v); // Not defined. This is placeholder for unsupported types

TARM_IO_DLL_PUBLIC
unsigned short network_to_host(unsigned short v);

TARM_IO_DLL_PUBLIC
unsigned int network_to_host(unsigned int v);

TARM_IO_DLL_PUBLIC
unsigned long network_to_host(unsigned long v);

TARM_IO_DLL_PUBLIC
unsigned long long network_to_host(unsigned long long v);

// host_to_network
template <typename T>
T host_to_network(T v); // Not defined. This is placeholder for unsupported types

TARM_IO_DLL_PUBLIC
unsigned short host_to_network(unsigned short v);

TARM_IO_DLL_PUBLIC
unsigned int host_to_network(unsigned int v);

TARM_IO_DLL_PUBLIC
unsigned long host_to_network(unsigned long v);

TARM_IO_DLL_PUBLIC
unsigned long long host_to_network(unsigned long long v);

} // namespace io
} // namespace tarm

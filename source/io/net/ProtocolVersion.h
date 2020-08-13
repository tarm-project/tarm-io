/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "Export.h"
#include "TlsVersion.h"
#include "DtlsVersion.h"

namespace tarm {
namespace io {
namespace net {

TARM_IO_DLL_PUBLIC net::TlsVersion min_supported_tls_version();
TARM_IO_DLL_PUBLIC net::TlsVersion max_supported_tls_version();

TARM_IO_DLL_PUBLIC net::DtlsVersion min_supported_dtls_version();
TARM_IO_DLL_PUBLIC net::DtlsVersion max_supported_dtls_version();

} // namespace net
} // namespace io
} // namespace tarm

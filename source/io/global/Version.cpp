/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "Version.h"

#include <openssl/ssl.h>

namespace io {
namespace global {

TlsVersion min_supported_tls_version() {
#ifdef SSL_OP_NO_TLSv1
    return TlsVersion::V1_0;
#elif defined(SSL_OP_NO_TLSv1_1)
    return TlsVersion::V1_1;
#elif defined(SSL_OP_NO_TLSv1_2)
    return TlsVersion::V1_2;
#else
    return TlsVersion::V1_3;
#endif
}

TlsVersion max_supported_tls_version() {
#ifdef SSL_OP_NO_TLSv1_3
    return TlsVersion::V1_3;
#elif defined(SSL_OP_NO_TLSv1_2)
    return TlsVersion::V1_2;
#elif defined(SSL_OP_NO_TLSv1_1)
    return TlsVersion::V1_1;
#else
    return TlsVersion::V1_0;
#endif
}

DtlsVersion min_supported_dtls_version() {
#ifdef SSL_OP_NO_DTLSv1
    return DtlsVersion::V1_0;
#else
    return DtlsVersion::V1_2;
#endif
}

DtlsVersion max_supported_dtls_version() {
#ifdef SSL_OP_NO_DTLSv1_2
    return DtlsVersion::V1_2;
#else
    return DtlsVersion::V1_0;
#endif
}

} // namespace global
} // namespace io

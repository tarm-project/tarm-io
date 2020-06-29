/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "Version.h"

#include <openssl/ssl.h>

namespace tarm {
namespace io {
namespace global {

net::TlsVersion min_supported_tls_version() {
#ifdef SSL_OP_NO_TLSv1
    return net::TlsVersion::V1_0;
#elif defined(SSL_OP_NO_TLSv1_1)
    return net::TlsVersion::V1_1;
#elif defined(SSL_OP_NO_TLSv1_2)
    return net::TlsVersion::V1_2;
#else
    return net::TlsVersion::V1_3;
#endif
}

net::TlsVersion max_supported_tls_version() {
#ifdef SSL_OP_NO_TLSv1_3
    return net::TlsVersion::V1_3;
#elif defined(SSL_OP_NO_TLSv1_2)
    return net::TlsVersion::V1_2;
#elif defined(SSL_OP_NO_TLSv1_1)
    return net::TlsVersion::V1_1;
#else
    return net::TlsVersion::V1_0;
#endif
}

net::DtlsVersion min_supported_dtls_version() {
#ifdef DTLS1_VERSION
    return net::DtlsVersion::V1_0;
#else
    return net::DtlsVersion::V1_2;
#endif
}

net::DtlsVersion max_supported_dtls_version() {
#ifdef DTLS1_2_VERSION
    return net::DtlsVersion::V1_2;
#else
    return net::DtlsVersion::V1_0;
#endif
}

} // namespace global
} // namespace io
} // namespace tarm

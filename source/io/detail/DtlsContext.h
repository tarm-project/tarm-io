/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "io/DtlsVersion.h"

#include <openssl/ssl.h>

namespace io {
namespace detail {

struct DtlsContext {
    DtlsContext(::X509* c, ::EVP_PKEY* k, ::SSL_CTX* ctx, DtlsVersionRange v) :
        certificate(c),
        private_key(k),
        ssl_ctx(ctx),
        dtls_version_range(v) {
    }

    ::X509* certificate = nullptr;
    ::EVP_PKEY* private_key = nullptr;
    ::SSL_CTX* ssl_ctx = nullptr;
    DtlsVersionRange dtls_version_range = DEFAULT_DTLS_VERSION_RANGE;
};

} // namespace detail
} // namespace io

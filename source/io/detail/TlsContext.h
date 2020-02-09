#pragma once

#include "io/TlsVersion.h"

#include <openssl/ssl.h>

namespace io {
namespace detail {

struct TlsContext {
    TlsContext(::X509* c, ::EVP_PKEY* k, ::SSL_CTX* ctx, TlsVersionRange v) :
        certificate(c),
        private_key(k),
        ssl_ctx(ctx),
        tls_version_range(v) {
    }

    ::X509* certificate = nullptr;
    ::EVP_PKEY* private_key = nullptr;
    ::SSL_CTX* ssl_ctx = nullptr;
    TlsVersionRange tls_version_range = DEFAULT_TLS_VERSION_RANGE;
};

} // namespace detail
} // namespace io

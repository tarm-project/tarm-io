#pragma once

#include "io/TlsVersion.h"

#include <openssl/ssl.h>

namespace io {
namespace detail {

struct TlsContext {
    TlsContext(::X509* c, ::EVP_PKEY* k, TlsVersionRange v) :
        certificate(c),
        private_key(k),
        tls_version_range(v) {
    }

    ::X509* certificate = nullptr;
    ::EVP_PKEY* private_key = nullptr;
    TlsVersionRange tls_version_range = DEFAULT_TLS_VERSION_RANGE;
};

} // namespace detail
} // namespace io

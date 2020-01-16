#pragma once

#include "io/DtlsVersion.h"

#include <openssl/ssl.h>

namespace io {
namespace detail {

struct DtlsContext {
    DtlsContext(::X509* c, ::EVP_PKEY* k, DtlsVersionRange v) :
        certificate(c),
        private_key(k),
        dtls_version_range(v) {
    }

    ::X509* certificate = nullptr;
    ::EVP_PKEY* private_key = nullptr;
    DtlsVersionRange dtls_version_range = DEFAULT_DTLS_VERSION_RANGE;
};

} // namespace detail
} // namespace io

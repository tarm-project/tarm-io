/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "io/TlsVersion.h"
#include "io/DtlsVersion.h"
#include "io/global/Configuration.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <memory>

namespace io {
namespace detail {

template<typename ParentType, typename ImplType>
class OpenSslContext {
public:
    OpenSslContext(EventLoop& loop, ParentType& parent) :
        m_parent(&parent),
        m_loop(&loop),
        m_ssl_ctx(nullptr, &::SSL_CTX_free) {
    }

    virtual ~OpenSslContext() = default;

    Error init_ssl_context(const SSL_METHOD* method);
    Error set_tls_version(TlsVersion version_min, TlsVersion version_max);
    Error set_dtls_version(DtlsVersion version_min, DtlsVersion version_max);
    Error ssl_init_certificate_and_key(::X509* certificate, ::EVP_PKEY* key);

    ::SSL_CTX* ssl_ctx();

protected:
    using SSL_CTXPtr = std::unique_ptr<::SSL_CTX, decltype(&::SSL_CTX_free)>;

    template<typename VersionType,
             void(OpenSslContext<ParentType, ImplType>::*EnableMethod)(VersionType),
             void(OpenSslContext<ParentType, ImplType>::*DisableMethod)(VersionType)>
    Error set_tls_dtls_version(VersionType version_min, VersionType version_max);

    void enable_tls_version(TlsVersion version);
    void disable_tls_version(TlsVersion version);
    void enable_dtls_version(DtlsVersion version);
    void disable_dtls_version(DtlsVersion version);

    ParentType* m_parent;
    EventLoop* m_loop;

    SSL_CTXPtr m_ssl_ctx;
};

///////////////////////////////////////// implementation ///////////////////////////////////////////

template<typename ParentType, typename ImplType>
::SSL_CTX* OpenSslContext<ParentType, ImplType>::ssl_ctx() {
    return m_ssl_ctx.get();
}

template<typename ParentType, typename ImplType>
template<typename VersionType,
         void(OpenSslContext<ParentType, ImplType>::*EnableMethod)(VersionType),
         void(OpenSslContext<ParentType, ImplType>::*DisableMethod)(VersionType)>
Error OpenSslContext<ParentType, ImplType>::set_tls_dtls_version(VersionType version_min, VersionType version_max) {
    if (version_min > version_max) {
        return IO_MAKE_LOGGED_ERROR(m_loop, m_parent, StatusCode::OPENSSL_ERROR, "Version mismatch. Minimum version is greater than maximum");
    }

    if (version_min != VersionType::MIN) {
        for (std::size_t v = static_cast<std::size_t>(TlsVersion::MIN); v < static_cast<std::size_t>(version_min); ++v) {
            (this->*DisableMethod)(static_cast<VersionType>(v));
        }
    }

    if (version_max != VersionType::MAX) {
        for (std::size_t v = static_cast<std::size_t>(version_max) + 1; v <= static_cast<std::size_t>(VersionType::MAX); ++v) {
            (this->*DisableMethod)(static_cast<VersionType>(v));
        }
    }

    for (std::size_t v = static_cast<std::size_t>(version_min); v <= static_cast<std::size_t>(version_max); ++v) {
        (this->*EnableMethod)(static_cast<VersionType>(v));
    }

    return Error(StatusCode::OK);
}

template<typename ParentType, typename ImplType>
Error OpenSslContext<ParentType, ImplType>::set_tls_version(TlsVersion version_min, TlsVersion version_max) {
    SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_SSLv2);
    SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_SSLv3);

    return this->set_tls_dtls_version<TlsVersion,
                                      &OpenSslContext<ParentType, ImplType>::enable_tls_version,
                                      &OpenSslContext<ParentType, ImplType>::disable_tls_version>
        (version_min, version_max);
}


template<typename ParentType, typename ImplType>
Error OpenSslContext<ParentType, ImplType>::set_dtls_version(DtlsVersion version_min, DtlsVersion version_max) {
    return this->set_tls_dtls_version<DtlsVersion,
                                      &OpenSslContext<ParentType, ImplType>::enable_dtls_version,
                                      &OpenSslContext<ParentType, ImplType>::disable_dtls_version>
    (version_min, version_max);
}

// Note could not generalize 'enable_tls_version' and 'disable_tls_version' because in earlier versions of OpenSSL
// SSL_CTX_clear_options and SSL_CTX_set_options were macros and in modern versions they are functions
template<typename ParentType, typename ImplType>
void OpenSslContext<ParentType, ImplType>::enable_tls_version(TlsVersion version) {
    switch (version) {
#ifdef TLS1_VERSION
        case TlsVersion::V1_0:
            SSL_CTX_clear_options(m_ssl_ctx.get(), SSL_OP_NO_TLSv1);
            break;
#endif
#ifdef TLS1_1_VERSION
        case TlsVersion::V1_1:
            SSL_CTX_clear_options(m_ssl_ctx.get(), SSL_OP_NO_TLSv1_1);
            break;
#endif
#ifdef TLS1_2_VERSION
        case TlsVersion::V1_2:
            SSL_CTX_clear_options(m_ssl_ctx.get(), SSL_OP_NO_TLSv1_2);
            break;
#endif
#ifdef TLS1_3_VERSION
        case TlsVersion::V1_3:
            SSL_CTX_clear_options(m_ssl_ctx.get(), SSL_OP_NO_TLSv1_3);
            break;
#endif
        default:
            // do nothing
            break;
    }
}

template<typename ParentType, typename ImplType>
void OpenSslContext<ParentType, ImplType>::disable_tls_version(TlsVersion version) {
    switch (version) {
#ifdef TLS1_VERSION
        case TlsVersion::V1_0:
            SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_TLSv1);
            break;
#endif
#ifdef TLS1_1_VERSION
        case TlsVersion::V1_1:
            SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_TLSv1_1);
            break;
#endif
#ifdef TLS1_2_VERSION
        case TlsVersion::V1_2:
            SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_TLSv1_2);
            break;
#endif
#ifdef TLS1_3_VERSION
        case TlsVersion::V1_3:
            SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_TLSv1_3);
            break;
#endif
        default:
            // do nothing
            break;
    }
}

template<typename ParentType, typename ImplType>
void OpenSslContext<ParentType, ImplType>::enable_dtls_version(DtlsVersion version) {
    switch (version) {
#ifdef SSL_OP_NO_DTLSv1
        case DtlsVersion::V1_0:
            SSL_CTX_clear_options(m_ssl_ctx.get(), SSL_OP_NO_DTLSv1);
            break;
#endif
#ifdef DTLS1_2_VERSION
        case DtlsVersion::V1_2:
            SSL_CTX_clear_options(m_ssl_ctx.get(), SSL_OP_NO_DTLSv1_2);
            break;
#endif
        default:
            // do nothing
            break;
    }
}

template<typename ParentType, typename ImplType>
void OpenSslContext<ParentType, ImplType>::disable_dtls_version(DtlsVersion version) {
    switch (version) {
#ifdef SSL_OP_NO_DTLSv1
        case DtlsVersion::V1_0:
            SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_DTLSv1);
            break;
#endif
#ifdef DTLS1_2_VERSION
        case DtlsVersion::V1_2:
            SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_DTLSv1_2);
            break;
#endif
        default:
            // do nothing
            break;
    }
}

template<typename ParentType, typename ImplType>
Error OpenSslContext<ParentType, ImplType>::init_ssl_context(const SSL_METHOD* method) {
    m_ssl_ctx.reset(SSL_CTX_new(method));
    if (m_ssl_ctx == nullptr) {
        return IO_MAKE_LOGGED_ERROR(m_loop, m_parent, StatusCode::OPENSSL_ERROR, "Failed to init SSL context");
    }

    SSL_CTX_set_verify(m_ssl_ctx.get(), SSL_VERIFY_NONE, NULL);
    // SSL_CTX_set_verify_depth

    auto cipher_result = SSL_CTX_set_cipher_list(m_ssl_ctx.get(), global::ciphers_list().c_str());
    if (cipher_result == 0) {
        return IO_MAKE_LOGGED_ERROR(m_loop, m_parent, StatusCode::OPENSSL_ERROR, "Failed to set ciphers list");
    }

    return StatusCode::OK;
}

template<typename ParentType, typename ImplType>
Error OpenSslContext<ParentType, ImplType>::ssl_init_certificate_and_key(::X509* certificate, ::EVP_PKEY* key) {
    auto result = SSL_CTX_use_certificate(this->ssl_ctx(), certificate);
    if (!result) {
        return IO_MAKE_LOGGED_ERROR(m_loop, m_parent, StatusCode::OPENSSL_ERROR, "Failed to load certificate");
    }

    result = SSL_CTX_use_PrivateKey(this->ssl_ctx(), key);
    if (!result) {
        return IO_MAKE_LOGGED_ERROR(m_loop, m_parent, StatusCode::OPENSSL_ERROR, "Failed to load private key");
    }

    result = SSL_CTX_check_private_key(this->ssl_ctx());
    if (!result) {
        return IO_MAKE_LOGGED_ERROR(m_loop, m_parent, StatusCode::OPENSSL_ERROR, "Failed to check private key");
    }

    return StatusCode::OK;
}

} // namespace detail
} // namespace io

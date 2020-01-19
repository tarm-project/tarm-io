#pragma once

#include "io/DataChunk.h"
#include "io/DtlsVersion.h"
#include "io/EventLoop.h"
#include "io/TlsVersion.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

//#include <memory>
//#include <assert.h>
#include <iostream>

namespace io {
namespace detail {

template<typename ParentType, typename ImplType>
class OpenSslClientImplBase {
public:
    OpenSslClientImplBase(EventLoop& loop, ParentType& parent);
    virtual ~OpenSslClientImplBase();

    bool schedule_removal();

    Error ssl_init();
    bool is_ssl_inited() const;

    virtual const SSL_METHOD* ssl_method() = 0;
    virtual bool ssl_set_siphers() = 0;
    virtual void ssl_set_versions() = 0;
    virtual bool ssl_init_certificate_and_key() = 0;
    virtual void ssl_set_state() = 0;

    Error set_tls_version(TlsVersion version_min, TlsVersion version_max);
    Error set_dtls_version(DtlsVersion version_min, DtlsVersion version_max);

    void read_from_ssl();
    virtual void on_ssl_read(const DataChunk&) = 0;

    void do_handshake();
    void handshake_read_from_sll_and_send();
    virtual void on_handshake_complete() = 0;

    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, typename ParentType::EndSendCallback callback);
    void send_data(const std::string& message, typename ParentType::EndSendCallback callback);

    void on_data_receive(const char* buf, std::size_t size);

    bool is_open() const;

    TlsVersion negotiated_tls_version() const;
    DtlsVersion negotiated_dtls_version() const;

protected:
    template<typename VersionType,
             void(OpenSslClientImplBase<ParentType, ImplType>::*EnableMethod)(VersionType),
             void(OpenSslClientImplBase<ParentType, ImplType>::*DisableMethod)(VersionType)>
    Error set_tls_dtls_version(VersionType version_min, VersionType version_max);

    void enable_tls_version(TlsVersion version);
    void disable_tls_version(TlsVersion version);
    void enable_dtls_version(DtlsVersion version);
    void disable_dtls_version(DtlsVersion version);

    ParentType* m_parent;
    EventLoop* m_loop;

    typename ParentType::UnderlyingClientType* m_client = nullptr;

    using SSLPtr = std::unique_ptr<::SSL, decltype(&::SSL_free)>;
    using SSL_CTXPtr = std::unique_ptr<::SSL_CTX, decltype(&::SSL_CTX_free)>;

    ::SSL* ssl();
    ::SSL_CTX* ssl_ctx();

    BIO* m_ssl_read_bio = nullptr;
    BIO* m_ssl_write_bio = nullptr;

    bool m_ssl_handshake_complete = false;

    // Removal is scheduled in 2 steps. First underlying connection is removed, then secure one.
    bool m_ready_schedule_removal = false;

    bool m_ssl_inited = false;

private:
    SSLPtr m_ssl;
    SSL_CTXPtr m_ssl_ctx;
};

///////////////////////////////////////// implementation ///////////////////////////////////////////

template<typename ParentType, typename ImplType>
OpenSslClientImplBase<ParentType, ImplType>::OpenSslClientImplBase(EventLoop& loop, ParentType& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_ssl(nullptr, &::SSL_free),
    m_ssl_ctx(nullptr, &::SSL_CTX_free) {
}

template<typename ParentType, typename ImplType>
OpenSslClientImplBase<ParentType, ImplType>::~OpenSslClientImplBase() {
}

/*
template<typename ParentType, typename ImplType>
Error OpenSslClientImplBase<ParentType, ImplType>::set_tls_version(TlsVersion version_min, TlsVersion version_max) {
    if (version_min > version_max) {
        // TODO: return error
    }

    if (version_min != TlsVersion::MIN) {
        for (std::size_t v = static_cast<std::size_t>(TlsVersion::MIN); v < static_cast<std::size_t>(version_min); ++v) {
            disable_tls_version(static_cast<TlsVersion>(v));
        }
    }

    if (version_max != TlsVersion::MAX) {
        for (std::size_t v = static_cast<std::size_t>(version_max) + 1; v <= static_cast<std::size_t>(TlsVersion::MAX); ++v) {
            disable_tls_version(static_cast<TlsVersion>(v));
        }
    }

    for (std::size_t v = static_cast<std::size_t>(version_min); v <= static_cast<std::size_t>(version_max); ++v) {
        enable_tls_version(static_cast<TlsVersion>(v));
    }

    return Error(StatusCode::OK);
}
*/

template<typename ParentType, typename ImplType>
template<typename VersionType,
         void(OpenSslClientImplBase<ParentType, ImplType>::*EnableMethod)(VersionType),
         void(OpenSslClientImplBase<ParentType, ImplType>::*DisableMethod)(VersionType)>
Error OpenSslClientImplBase<ParentType, ImplType>::set_tls_dtls_version(VersionType version_min, VersionType version_max) {
    if (version_min > version_max) {
        // TODO: return error
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
Error OpenSslClientImplBase<ParentType, ImplType>::set_tls_version(TlsVersion version_min, TlsVersion version_max) {
    SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_SSLv2);
    SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_SSLv3);

    return this->set_tls_dtls_version<TlsVersion,
                                      &OpenSslClientImplBase<ParentType, ImplType>::enable_tls_version,
                                      &OpenSslClientImplBase<ParentType, ImplType>::disable_tls_version>
        (version_min, version_max);
}


template<typename ParentType, typename ImplType>
Error OpenSslClientImplBase<ParentType, ImplType>::set_dtls_version(DtlsVersion version_min, DtlsVersion version_max) {
    return this->set_tls_dtls_version<DtlsVersion,
                                      &OpenSslClientImplBase<ParentType, ImplType>::enable_dtls_version,
                                      &OpenSslClientImplBase<ParentType, ImplType>::disable_dtls_version>
    (version_min, version_max);
}

// Note could not generalize 'enable_tls_version' and 'disable_tls_version' because in earlier versions of OpenSSl
// SSL_CTX_clear_options and SSL_CTX_set_options were macros and in modern versions they are functions
template<typename ParentType, typename ImplType>
void OpenSslClientImplBase<ParentType, ImplType>::enable_tls_version(TlsVersion version) {
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
void OpenSslClientImplBase<ParentType, ImplType>::disable_tls_version(TlsVersion version) {
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
void OpenSslClientImplBase<ParentType, ImplType>::enable_dtls_version(DtlsVersion version) {
    switch (version) {
#ifdef DTLS1_VERSION
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
void OpenSslClientImplBase<ParentType, ImplType>::disable_dtls_version(DtlsVersion version) {
    switch (version) {
#ifdef DTLS1_VERSION
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
TlsVersion OpenSslClientImplBase<ParentType, ImplType>::negotiated_tls_version() const {
    if (!is_open()) {
        return TlsVersion::UNKNOWN;
    }

    SSL_SESSION* session = SSL_get_session(m_ssl.get());
    if (session == nullptr) {
        return TlsVersion::UNKNOWN;
    }

    switch (session->ssl_version) {
#ifdef TLS1_VERSION
        case TLS1_VERSION:
            return TlsVersion::V1_0;
#endif
#ifdef TLS1_1_VERSION
        case TLS1_1_VERSION:
            return TlsVersion::V1_1;
#endif
#ifdef TLS1_2_VERSION
        case TLS1_2_VERSION:
            return TlsVersion::V1_2;
#endif
#ifdef TLS1_3_VERSION
        case TLS1_3_VERSION:
            return TlsVersion::V1_3;
#endif
        default:
            return TlsVersion::UNKNOWN;
    }
}

template<typename ParentType, typename ImplType>
DtlsVersion OpenSslClientImplBase<ParentType, ImplType>::negotiated_dtls_version() const {
    if (!is_open()) {
        return DtlsVersion::UNKNOWN;
    }

    SSL_SESSION* session = SSL_get_session(m_ssl.get());
    if (session == nullptr) {
        return DtlsVersion::UNKNOWN;
    }

    switch (m_ssl.get()->version) {
#ifdef DTLS1_VERSION
        case DTLS1_VERSION:
            return DtlsVersion::V1_0;
#endif
#ifdef DTLS1_2_VERSION
        case DTLS1_2_VERSION:
            return DtlsVersion::V1_2;
#endif
        default:
            return DtlsVersion::UNKNOWN;
    }
}

template<typename ParentType, typename ImplType>
::SSL* OpenSslClientImplBase<ParentType, ImplType>::ssl() {
    return m_ssl.get();
}

template<typename ParentType, typename ImplType>
::SSL_CTX* OpenSslClientImplBase<ParentType, ImplType>::ssl_ctx() {
    return m_ssl_ctx.get();
}

template<typename ParentType, typename ImplType>
bool OpenSslClientImplBase<ParentType, ImplType>::is_open() const {
    return m_client && m_client->is_open();
}

template<typename ParentType, typename ImplType>
Error OpenSslClientImplBase<ParentType, ImplType>::ssl_init() {
    // TOOD: looks like this could be done only once
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    int result = SSL_library_init();

    //SSL_load_error_strings();
    //OpenSSL_add_ssl_algorithms();
    if (!result) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to init OpenSSL");
        return Error(StatusCode::OPENSSL_ERROR, "Failed to init OpenSSL");
    }

    //OpenSSL_add_all_algorithms();
    //OpenSSL_add_all_ciphers();
    //OpenSSL_add_all_digests();

    m_ssl_ctx.reset(SSL_CTX_new(ssl_method()));
    if (m_ssl_ctx == nullptr) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to init SSL context");
        return Error(StatusCode::OPENSSL_ERROR, "Failed to init SSL context");
    }

    ssl_set_versions();
    //SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_TLSv1_2);
    //set_tls_version(io::TlsVersion::V1_0, io::TlsVersion::V1_2);

    // TODO: remove ???
    //SSL_CTX_set_ecdh_auto(m_ssl_ctx, 1);

    // TODO: implement verify?
    SSL_CTX_set_verify(m_ssl_ctx.get(), SSL_VERIFY_NONE, NULL);
    // SSL_CTX_set_verify_depth

    if (!ssl_set_siphers()) {
        return Error(StatusCode::OPENSSL_ERROR, "Failed to set siphers");
    }

    if (!ssl_init_certificate_and_key()) {
        return Error(StatusCode::OPENSSL_ERROR, "Failed to init certificate and key");
    }

    m_ssl.reset(SSL_new(m_ssl_ctx.get()));
    if (m_ssl == nullptr) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to create SSL");
        return Error(StatusCode::OPENSSL_ERROR, "Failed to create SSL");
    }

    m_ssl_read_bio = BIO_new(BIO_s_mem());
    if (m_ssl_read_bio == nullptr) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to create read BIO");
        return Error(StatusCode::OPENSSL_ERROR, "Failed to create read BIO");
    }

    m_ssl_write_bio = BIO_new(BIO_s_mem());
    if (m_ssl_write_bio == nullptr) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to create write BIO");
        return Error(StatusCode::OPENSSL_ERROR, "Failed to create write BIO");
    }

    SSL_set_bio(m_ssl.get(), m_ssl_read_bio, m_ssl_write_bio);

    ssl_set_state();

    IO_LOG(m_loop, DEBUG, m_parent, "SSL inited");
    m_ssl_inited = true;

    return StatusCode::OK;
}

template<typename ParentType, typename ImplType>
bool OpenSslClientImplBase<ParentType, ImplType>::is_ssl_inited() const {
    return m_ssl_inited;
}

template<typename ParentType, typename ImplType>
void OpenSslClientImplBase<ParentType, ImplType>::read_from_ssl() {
    // TODO: investigate if this buffer can be of less size
    // TODO: not create this buffer on every read
    const std::size_t SIZE = 16*1024; // https://www.openssl.org/docs/man1.0.2/man3/SSL_read.html
    std::shared_ptr<char> decrypted_buf(new char[SIZE], std::default_delete<char[]>());
    // TODO: TEST save buffer in callback and check if it is valid

    int decrypted_size = SSL_read(m_ssl.get(), decrypted_buf.get(), SIZE);
    while (decrypted_size > 0) {
        IO_LOG(m_loop, TRACE, m_parent, "Decrypted message of size:", decrypted_size);
        on_ssl_read({decrypted_buf, static_cast<std::size_t>(decrypted_size)});
        decrypted_size = SSL_read(m_ssl.get(), decrypted_buf.get(), SIZE);
    }

    if (decrypted_size < 0) {
        int code = SSL_get_error(m_ssl.get(), decrypted_size);
        if (code != SSL_ERROR_WANT_READ) {
            IO_LOG(m_loop, ERROR, m_parent, "Failed to write buf of size", code);
            // TODO: handle error
            return;
        }
    }
}

template<typename ParentType, typename ImplType>
void OpenSslClientImplBase<ParentType, ImplType>::handshake_read_from_sll_and_send() {
    const std::size_t BUF_SIZE = 4096;
    std::shared_ptr<char> buf(new char[BUF_SIZE], [](const char* p) { delete[] p; });
    const auto size = BIO_read(m_ssl_write_bio, buf.get(), BUF_SIZE);

    IO_LOG(m_loop, TRACE, m_parent, "Getting data from SSL and sending to server, size:", size);
    m_client->send_data(buf, size);
}

template<typename ParentType, typename ImplType>
void OpenSslClientImplBase<ParentType, ImplType>::do_handshake() {
    IO_LOG(m_loop, DEBUG, m_parent, "Doing handshake");

    auto handshake_result = SSL_do_handshake(m_ssl.get());

    int write_pending = BIO_pending(m_ssl_write_bio);
    int read_pending = BIO_pending(m_ssl_read_bio);
    IO_LOG(m_loop, TRACE, m_parent, "write_pending:", write_pending);
    IO_LOG(m_loop, TRACE, m_parent, "read_pending:", read_pending);

    if (handshake_result < 0) {
        auto error = SSL_get_error(m_ssl.get(), handshake_result);

        if (error == SSL_ERROR_WANT_READ) {
            IO_LOG(m_loop, TRACE, m_parent, "SSL_ERROR_WANT_READ");

            handshake_read_from_sll_and_send();
        } else if (error == SSL_ERROR_WANT_WRITE) {
            IO_LOG(m_loop, TRACE, m_parent, "SSL_ERROR_WANT_WRITE");
        } else {
            char msg[1024];
            ERR_error_string_n(ERR_get_error(), msg, sizeof(msg));
            printf("%s %s %s %s\n", msg, ERR_lib_error_string(0), ERR_func_error_string(0), ERR_reason_error_string(0));

            // TODO: error handling here
        }
    } else if (handshake_result == 1) {
        IO_LOG(m_loop, DEBUG, m_parent, "Connected!");

        if (write_pending) {
            handshake_read_from_sll_and_send();
        }

        m_ssl_handshake_complete = true;

        on_handshake_complete();

        if (read_pending) {
            read_from_ssl();
        }
    } else {
        IO_LOG(m_loop, ERROR, m_parent, "The TLS/SSL handshake was not successful but was shut down controlled and by the specifications of the TLS/SSL protocol.");
        // TODO: error handling
    }
}

template<typename ParentType, typename ImplType>
void OpenSslClientImplBase<ParentType, ImplType>::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, typename ParentType::EndSendCallback callback) {

    if (!is_open()) {
        callback(*m_parent, Error(StatusCode::SOCKET_IS_NOT_CONNECTED));
        return;
    }

    const auto write_result = SSL_write(m_ssl.get(), buffer.get(), size);
    if (write_result <= 0) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to write buf of size", size);

        const auto openss_error_code = ERR_get_error();
        if (callback) {
            callback(*m_parent, Error(StatusCode::OPENSSL_ERROR, ERR_reason_error_string(openss_error_code)));
        }

        return;
    }

    // TODO: not allocate memory on each write???
    const std::size_t SIZE = BIO_pending(m_ssl_write_bio);
    std::shared_ptr<char> ptr(new char[SIZE], [](const char* p) { delete[] p;});

    const auto actual_size = BIO_read(m_ssl_write_bio, ptr.get(), SIZE);
    if (actual_size < 0) {
        IO_LOG(m_loop, ERROR, m_parent, "BIO_read failed code:", actual_size);
        // TODO: error handling???
        return;
    }

    IO_LOG(m_loop, TRACE, m_parent, "Sending message to client. Original size:", size, "encrypted_size:", actual_size);
    m_client->send_data(ptr, actual_size, [callback, this](typename ParentType::UnderlyingClientType& tcp_client, const Error& error) {
        if (callback) {
            callback(*m_parent, error);
        }
    });
}

template<typename ParentType, typename ImplType>
void OpenSslClientImplBase<ParentType, ImplType>::send_data(const std::string& message, typename ParentType::EndSendCallback callback) {
    std::shared_ptr<char> ptr(new char[message.size()], [](const char* p) { delete[] p;});
    std::copy(message.c_str(), message.c_str() + message.size(), ptr.get());
    send_data(ptr, static_cast<std::uint32_t>(message.size()), callback);
}

template<typename ParentType, typename ImplType>
void OpenSslClientImplBase<ParentType, ImplType>::on_data_receive(const char* buf, std::size_t size) {
    IO_LOG(m_loop, TRACE, m_parent, "");

    if (m_ssl_handshake_complete) {
        const auto written_size = BIO_write(m_ssl_read_bio, buf, size);
        if (written_size < 0) {
            IO_LOG(m_loop, ERROR, m_parent, "BIO_write failed with code:", written_size);
            return;
        }

        read_from_ssl();
    } else {
        const auto write_size = BIO_write(m_ssl_read_bio, buf, size);
        // TODO: error handling
        do_handshake();
    }
}

template<typename ParentType, typename ImplType>
bool OpenSslClientImplBase<ParentType, ImplType>::schedule_removal() {
    IO_LOG(m_loop, TRACE, "");

    if (m_client) {
        if (!m_ready_schedule_removal) {
            m_client->set_on_schedule_removal([this](const Removable&) {
                this->m_parent->schedule_removal();
            });
            m_ready_schedule_removal = true;
            m_client->schedule_removal();
            return false; // postpone removal
        }
    }

    return true;
}

} // namespace detail
} // namespace io

#pragma once

#include "io/EventLoop.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

//#include <memory>
//#include <assert.h>

namespace io {
namespace detail {

template<typename ParentType, typename ImplType>
class TlsTcpClientImplBase {
public:
    TlsTcpClientImplBase(EventLoop& loop, ParentType& parent);
    ~TlsTcpClientImplBase();

    bool ssl_init();
    virtual const SSL_METHOD* ssl_method() = 0;
    virtual bool ssl_set_siphers() = 0;
    virtual bool ssl_init_certificate_and_key() = 0;
    virtual void ssl_set_state() = 0;

protected:
    EventLoop* m_loop;

    SSL_CTX* m_ssl_ctx = nullptr;
    BIO* m_ssl_read_bio = nullptr;
    BIO* m_ssl_write_bio = nullptr;
    SSL* m_ssl = nullptr;

    bool m_ssl_handshake_complete = false;
};

///////////////////////////////////////// implementation ///////////////////////////////////////////

template<typename ParentType, typename ImplType>
TlsTcpClientImplBase<ParentType, ImplType>::TlsTcpClientImplBase(EventLoop& loop, ParentType& parent) :
    m_loop(&loop) {
}

template<typename ParentType, typename ImplType>
TlsTcpClientImplBase<ParentType, ImplType>::~TlsTcpClientImplBase() {
    // TODO: smart pointers???
    if (m_ssl) {
        SSL_free(m_ssl);
    }

    if (m_ssl_ctx) {
        SSL_CTX_free(m_ssl_ctx);
    }
}

template<typename ParentType, typename ImplType>
bool TlsTcpClientImplBase<ParentType, ImplType>::ssl_init() {
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    int result = SSL_library_init();

    //SSL_load_error_strings();
    //OpenSSL_add_ssl_algorithms();
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to init OpenSSL");
        return false;
    }

    //OpenSSL_add_all_algorithms();
    //OpenSSL_add_all_ciphers();
    //OpenSSL_add_all_digests();

    // TODO: support different versions of TLS
    m_ssl_ctx = SSL_CTX_new(ssl_method()/*TLSv1_2_server_method()*/);
    if (m_ssl_ctx == nullptr) {
        IO_LOG(m_loop, ERROR, "Failed to init SSL context");
        return false;
    }

    // TODO: remove ???
    //SSL_CTX_set_ecdh_auto(m_ssl_ctx, 1);

    // TODO: remove this
    SSL_CTX_set_verify(m_ssl_ctx, SSL_VERIFY_NONE, NULL);

    if (!ssl_set_siphers()) {
        return false;
    }
    /*
    result = SSL_CTX_set_cipher_list(m_ssl_ctx, "ALL:!SHA256:!SHA384:!aPSK:!ECDSA+SHA1:!ADH:!LOW:!EXP:!MD5");
    if (result == 0) {
        IO_LOG(m_loop, ERROR, "Failed to set siphers list");
        return false;
    }
    */

    /*
    result = SSL_CTX_use_certificate(m_ssl_ctx, m_certificate);
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to load certificate");
        return false;
    }

    result = SSL_CTX_use_PrivateKey(m_ssl_ctx, m_private_key);
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to load private key");
        return false;
    }

    result = SSL_CTX_check_private_key(m_ssl_ctx);
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to check private key");
        return false;
    }
    */

   if (!ssl_init_certificate_and_key()) {
       return false;
   }

    // TODO: most likely need to set also
    // SSL_CTX_set_verify
    // SSL_CTX_set_verify_depth

    m_ssl = SSL_new(m_ssl_ctx);
    if (m_ssl == nullptr) {
        IO_LOG(m_loop, ERROR, "Failed to create SSL");
        return false;
    }

    m_ssl_read_bio = BIO_new(BIO_s_mem());
    if (m_ssl_read_bio == nullptr) {
        IO_LOG(m_loop, ERROR, "Failed to create read BIO");
        return false;
    }

    m_ssl_write_bio = BIO_new(BIO_s_mem());
    if (m_ssl_write_bio == nullptr) {
        IO_LOG(m_loop, ERROR, "Failed to create write BIO");
        return false;
    }

    SSL_set_bio(m_ssl, m_ssl_read_bio, m_ssl_write_bio);

    //SSL_set_accept_state(m_ssl);
    ssl_set_state();

    IO_LOG(m_loop, DEBUG, "SSL inited");

    return true;
}

} // namespace detail
} // namespace io
/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "io/DataChunk.h"
#include "io/DtlsVersion.h"
#include "io/EventLoop.h"
#include "io/TlsVersion.h"
#include "io/global/Configuration.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <iostream>

namespace io {
namespace detail {

template<typename ParentType, typename ImplType>
class OpenSslClientImplBase {
public:
    OpenSslClientImplBase(EventLoop& loop, ParentType& parent);
    virtual ~OpenSslClientImplBase();

    bool schedule_removal();

    Error ssl_init(::SSL_CTX* ssl_ctx);
    bool is_ssl_inited() const;

    virtual void ssl_set_state() = 0;

    void read_from_ssl();
    virtual void on_ssl_read(const DataChunk& data, const Error& error) = 0;

    void do_handshake();
    void finish_handshake();
    virtual void on_handshake_complete() = 0;
    virtual void on_handshake_failed(long openssl_error_code, const Error& error) = 0;

    virtual void on_alert(int code) = 0;

    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, typename ParentType::EndSendCallback callback);
    void send_data(const std::string& message, typename ParentType::EndSendCallback callback);

    void on_data_receive(const char* buf, std::size_t size);

    bool is_open() const;

    Error ssl_shutdown(typename ParentType::UnderlyingClientType::EndSendCallback on_send);

    TlsVersion negotiated_tls_version() const;
    DtlsVersion negotiated_dtls_version() const;

    const Endpoint& endpoint() const;

protected:
    void internal_read_from_sll_and_send(typename ParentType::UnderlyingClientType::EndSendCallback on_send);

    ParentType* m_parent;
    EventLoop* m_loop;

    typename ParentType::UnderlyingClientType* m_client = nullptr;

    using SSLPtr = std::unique_ptr<::SSL, decltype(&::SSL_free)>;

    ::SSL* ssl();

    BIO* m_ssl_read_bio = nullptr;
    BIO* m_ssl_write_bio = nullptr;

    // TODO: move
    enum HandshakeState {
        NONE = 0,
        IN_PROGRESS,
        FINISHING,
        FINISHED
    };

    HandshakeState m_ssl_handshake_state = HandshakeState::NONE;

    // Removal is scheduled in 2 steps. First underlying connection is removed, then secure one.
    bool m_ready_schedule_removal = false;

    bool m_ssl_inited = false;

private:
    static void ssl_state_callback(const SSL* ssl, int where, int ret);

    SSLPtr m_ssl;

    // https://www.openssl.org/docs/man1.0.2/man3/SSL_read.html
    const std::size_t DECRYPT_BUF_SIZE = 16 * 1024;
    std::shared_ptr<char> m_decrypt_buf;
};

///////////////////////////////////////// implementation ///////////////////////////////////////////

template<typename ParentType, typename ImplType>
OpenSslClientImplBase<ParentType, ImplType>::OpenSslClientImplBase(EventLoop& loop, ParentType& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_ssl(nullptr, &::SSL_free),
    m_decrypt_buf(new char[DECRYPT_BUF_SIZE], std::default_delete<char[]>()) {
}

template<typename ParentType, typename ImplType>
OpenSslClientImplBase<ParentType, ImplType>::~OpenSslClientImplBase() {
}

template<typename ParentType, typename ImplType>
TlsVersion OpenSslClientImplBase<ParentType, ImplType>::negotiated_tls_version() const {
    if (!is_open()) {
        return TlsVersion::UNKNOWN;
    }

    if (m_ssl_handshake_state < HandshakeState::FINISHING) {
        return TlsVersion::UNKNOWN;
    }

    switch (SSL_version(m_ssl.get())) {
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

    if (m_ssl_handshake_state < HandshakeState::FINISHING) {
        return DtlsVersion::UNKNOWN;
    }

    switch (SSL_version(m_ssl.get())) {
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
bool OpenSslClientImplBase<ParentType, ImplType>::is_open() const {
    return m_client && m_client->is_open() && m_ssl_handshake_state == HandshakeState::FINISHED;
}

template<typename ParentType, typename ImplType>
Error OpenSslClientImplBase<ParentType, ImplType>::ssl_init(::SSL_CTX* ssl_ctx) {
    m_ssl.reset(SSL_new(ssl_ctx));
    if (m_ssl == nullptr) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to create SSL");
        return Error(StatusCode::OPENSSL_ERROR, "Failed to create SSL");
    }

    SSL_set_ex_data(m_ssl.get(), 0, this);
    SSL_set_info_callback(m_ssl.get(), &OpenSslClientImplBase<ParentType, ImplType>::ssl_state_callback);

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
    int decrypted_size = SSL_read(m_ssl.get(), m_decrypt_buf.get(), DECRYPT_BUF_SIZE);
    std::size_t counter = 0;
    while (decrypted_size > 0) {
        IO_LOG(m_loop, TRACE, m_parent, "Decrypted message of size:", decrypted_size);
        const auto prev_use_count = m_decrypt_buf.use_count();
        on_ssl_read({m_decrypt_buf, static_cast<std::size_t>(decrypted_size)}, StatusCode::OK);
        if (prev_use_count != m_decrypt_buf.use_count()) { // user made a copy
            m_decrypt_buf.reset(new char[DECRYPT_BUF_SIZE], std::default_delete<char[]>());
        }
        decrypted_size = SSL_read(m_ssl.get(), m_decrypt_buf.get(), DECRYPT_BUF_SIZE);
        ++counter;
    }

    // TODO: fixme!!!111
    // Have incoming data, but no successful read opearions
    /*
    if (decrypted_size < 0 && counter == 0) {
        on_ssl_read({nullptr, 0}, Error(StatusCode::OPENSSL_ERROR, "failed to decrypt received data"));
        return;
    }
    */

    if (decrypted_size < 0) {
        int code = SSL_get_error(m_ssl.get(), decrypted_size);
        if (code != SSL_ERROR_WANT_READ) {
            const auto openssl_error_code = ERR_get_error();
            const char* str = ERR_reason_error_string(openssl_error_code);
            IO_LOG(m_loop, ERROR, m_parent, "Failed to write buf of size. Error code:", openssl_error_code, "message:", str ? str : "");
            on_ssl_read({nullptr, 0}, Error(StatusCode::OPENSSL_ERROR, str ? str : ""));
            return;
        }
    }
}

template<typename ParentType, typename ImplType>
void OpenSslClientImplBase<ParentType, ImplType>::internal_read_from_sll_and_send(typename ParentType::UnderlyingClientType::EndSendCallback on_send) {

    const auto write_pending = BIO_pending(m_ssl_write_bio);
    if (write_pending == 0) {
        IO_LOG(m_loop, WARNING, m_parent, "No data to read from OpenSSL BIO");
        return;
    }

    std::shared_ptr<char> buf(new char[write_pending], [](const char* p) { delete[] p; });
    const auto size = BIO_read(m_ssl_write_bio, buf.get(), write_pending);

    IO_LOG(m_loop, TRACE, m_parent, "Getting data from SSL and sending to server, size:", size);
    m_client->send_data(buf, size, on_send);
}

template<typename ParentType, typename ImplType>
void OpenSslClientImplBase<ParentType, ImplType>::do_handshake() {
    IO_LOG(m_loop, DEBUG, m_parent, "Doing handshake");

    if (m_ssl_handshake_state == HandshakeState::FINISHING) {
        IO_LOG(m_loop, TRACE, m_parent, "Handshake is in FINISHING state, return");
        return;
    }

    auto handshake_result = SSL_do_handshake(m_ssl.get());

    int write_pending = BIO_pending(m_ssl_write_bio);
    int read_pending = BIO_pending(m_ssl_read_bio);
    IO_LOG(m_loop, TRACE, m_parent, "write_pending:", write_pending);
    IO_LOG(m_loop, TRACE, m_parent, "read_pending:", read_pending);

    if (handshake_result < 0) {
        auto ssl_error = SSL_get_error(m_ssl.get(), handshake_result);

        if (ssl_error == SSL_ERROR_WANT_READ) {
            IO_LOG(m_loop, TRACE, m_parent, "SSL_ERROR_WANT_READ");
            if (write_pending == 0) {
                IO_LOG(m_loop, TRACE, m_parent, "Handshake failed because there is no valid data to read from OpenSSL BIO");
                on_handshake_failed(-1, Error(StatusCode::OPENSSL_ERROR, "Handshake failed, invalid data"));
                return;
            }

            internal_read_from_sll_and_send(
                [this](typename ParentType::UnderlyingClientType& client, const io::Error& error) {
                    if (error) {
                        on_handshake_failed(-1, error);
                    }
                }
            );
        } else if (ssl_error == SSL_ERROR_WANT_WRITE) {
            IO_LOG(m_loop, TRACE, m_parent, "SSL_ERROR_WANT_WRITE");
        } else {
            const auto openssl_error_code = ERR_get_error();
            IO_LOG(m_loop, ERROR, m_parent, "Handshake error:", openssl_error_code);
            if (write_pending) {
                // Just notification for other side without care about result
                internal_read_from_sll_and_send(nullptr);
            }

            const char* str = ERR_reason_error_string(openssl_error_code);
            on_handshake_failed(openssl_error_code, Error(StatusCode::OPENSSL_ERROR, str ? str : ""));
        }
    } else if (handshake_result == 1) {
        if (write_pending) {
            m_ssl_handshake_state = HandshakeState::FINISHING;

            internal_read_from_sll_and_send(
                [this, read_pending](typename ParentType::UnderlyingClientType& client, const io::Error& error) {
                    if (error) {
                        on_handshake_failed(-1, error);
                    } else {
                        finish_handshake();

                        if (read_pending) {
                            read_from_ssl();
                        }
                    }
                }
            );
        } else {
            finish_handshake();

            if (read_pending) {
                read_from_ssl();
            }
        }
    } else {
        const auto openssl_error_code = ERR_get_error();
        const char* str = ERR_reason_error_string(openssl_error_code);
        IO_LOG(m_loop, ERROR, m_parent, "The TLS/SSL handshake was not successful but was shut down controlled and by the specifications of the TLS/SSL protocol. Error code:", openssl_error_code, "message:", str ? str : "");
        on_handshake_failed(openssl_error_code, Error(StatusCode::OPENSSL_ERROR, str ? str : ""));
    }
}

template<typename ParentType, typename ImplType>
void OpenSslClientImplBase<ParentType, ImplType>::finish_handshake() {
    IO_LOG(m_loop, DEBUG, m_parent, "Connected!");
    m_ssl_handshake_state = HandshakeState::FINISHED;
    on_handshake_complete();
}

template<typename ParentType, typename ImplType>
void OpenSslClientImplBase<ParentType, ImplType>::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, typename ParentType::EndSendCallback callback) {

    if (!is_open()) {
        callback(*m_parent, Error(StatusCode::NOT_CONNECTED));
        return;
    }

    const auto write_result = SSL_write(m_ssl.get(), buffer.get(), size);
    if (write_result <= 0) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to write buf of size", size);

        const auto openssl_error_code = ERR_get_error();
        if (callback) {
            const char* str = ERR_reason_error_string(openssl_error_code);
            callback(*m_parent, Error(StatusCode::OPENSSL_ERROR, str ? str : ""));
        }

        return;
    }

    const std::size_t SIZE = BIO_pending(m_ssl_write_bio);
    std::shared_ptr<char> ptr(new char[SIZE], [](const char* p) { delete[] p;});

    const auto actual_size = BIO_read(m_ssl_write_bio, ptr.get(), SIZE);
    if (actual_size <= 0) {
        IO_LOG(m_loop, ERROR, m_parent, "BIO_read failed code:", actual_size);
        if (callback) {
            callback(*m_parent, Error(StatusCode::OPENSSL_ERROR, "Nothing to read from SSL"));
        }
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

    if (m_ssl_handshake_state == HandshakeState::FINISHED) {
        const auto write_size = BIO_write(m_ssl_read_bio, buf, size);
        if (write_size < 0) {
            IO_LOG(m_loop, ERROR, m_parent, "BIO_write failed with code:", write_size);
            return;
        }

        read_from_ssl();
    } else {
        const auto write_size = BIO_write(m_ssl_read_bio, buf, size);
        if (write_size <= 0) {
            // TODO: on handshake failed here???
            IO_LOG(m_loop, ERROR, m_parent, "BIO_write failed with code:", write_size);
            return;
        }

        do_handshake();
    }
}

template<typename ParentType, typename ImplType>
bool OpenSslClientImplBase<ParentType, ImplType>::schedule_removal() {
    IO_LOG(m_loop, TRACE, m_parent, "");

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

template<typename ParentType, typename ImplType>
Error OpenSslClientImplBase<ParentType, ImplType>::ssl_shutdown(typename ParentType::UnderlyingClientType::EndSendCallback on_send) {
    IO_LOG(m_loop, TRACE, m_parent, "");

    auto return_code = SSL_shutdown(m_ssl.get());
    if (return_code < 0) {
        const auto openssl_error_code = ERR_get_error();
        const char* str = ERR_reason_error_string(openssl_error_code);
        return Error(StatusCode::OPENSSL_ERROR, str ? str : "");
    }

    const auto write_pending = BIO_pending(m_ssl_write_bio);
    if (write_pending) {
        internal_read_from_sll_and_send(on_send);
    }

    return Error(0);
}

template<typename ParentType, typename ImplType>
void OpenSslClientImplBase<ParentType, ImplType>::ssl_state_callback(const SSL* ssl, int where, int ret) {
    auto& this_ = *reinterpret_cast<OpenSslClientImplBase*>(SSL_get_ex_data(ssl, 0));
    if (where & SSL_CB_ALERT) {
        if (!(SSL_get_shutdown(ssl) & SSL_SENT_SHUTDOWN)) {
            // Calling callback only on receiving alert
            this_.on_alert(ret & 0xFF);
        }
    }
}

template<typename ParentType, typename ImplType>
const Endpoint& OpenSslClientImplBase<ParentType, ImplType>::endpoint() const {
    return m_client->endpoint();
}

} // namespace detail
} // namespace io

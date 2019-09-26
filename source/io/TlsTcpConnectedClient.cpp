#include "TlsTcpConnectedClient.h"

#include "Common.h"
#include "TcpConnectedClient.h"
#include "detail/TlsTcpClientImplBase.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace io {

class TlsTcpConnectedClient::Impl : public detail::TlsTcpClientImplBase<TlsTcpConnectedClient, TlsTcpConnectedClient::Impl> {
public:
    Impl(EventLoop& loop, TlsTcpServer& tls_server, NewConnectionCallback new_connection_callback, X509* certificate, X509* private_key, TcpConnectedClient& tcp_client, TlsTcpConnectedClient& parent);
    ~Impl();

    void close();
    void shutdown();
    bool is_open() const;

    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback);
    void send_data(const std::string& message, EndSendCallback callback);

    void set_data_receive_callback(DataReceiveCallback callback);
    void on_data_receive(const char* buf, std::size_t size);

    //bool init_ssl();
    void do_handshake();

    //void read_from_ssl();

protected:
    const SSL_METHOD* ssl_method() override {
        return TLSv1_2_server_method();
    }

    bool ssl_set_siphers() override {
        auto result = SSL_CTX_set_cipher_list(m_ssl_ctx, "ALL:!SHA256:!SHA384:!aPSK:!ECDSA+SHA1:!ADH:!LOW:!EXP:!MD5");
        if (result == 0) {
            IO_LOG(m_loop, ERROR, "Failed to set siphers list");
            return false;
        }
        return true;
    }

    bool ssl_init_certificate_and_key() override {
        auto result = SSL_CTX_use_certificate(m_ssl_ctx, m_certificate);
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

        return true;
    }

    void ssl_set_state() override {
        SSL_set_accept_state(m_ssl);
    }

    void on_ssl_read(const char* buf, std::size_t size) override {
        if (m_data_receive_callback) {
            m_data_receive_callback(*m_tls_server, *m_parent, buf, size);
        }
    }

private:
    TlsTcpConnectedClient* m_parent;
    EventLoop* m_loop;

    TlsTcpServer* m_tls_server;

    ::X509* m_certificate;
    ::EVP_PKEY* m_private_key;

    DataReceiveCallback m_data_receive_callback = nullptr;
    NewConnectionCallback m_new_connection_callback = nullptr;
};

TlsTcpConnectedClient::Impl::Impl(EventLoop& loop,
                                  TlsTcpServer& tls_server,
                                  NewConnectionCallback new_connection_callback,
                                  X509* certificate,
                                  EVP_PKEY* private_key,
                                  TcpConnectedClient& tcp_client,
                                  TlsTcpConnectedClient& parent) :
    TlsTcpClientImplBase(loop, parent),
    m_parent(&parent),
    m_loop(&loop),
    m_tls_server(&tls_server),
    m_certificate(reinterpret_cast<::X509*>(certificate)),
    m_private_key(reinterpret_cast<::EVP_PKEY*>(private_key)),
    m_new_connection_callback(new_connection_callback) {
    m_tcp_client = &tcp_client;
    m_tcp_client->set_user_data(&parent);
}

TlsTcpConnectedClient::Impl::~Impl() {
}

void TlsTcpConnectedClient::Impl::set_data_receive_callback(DataReceiveCallback callback) {
    m_data_receive_callback = callback;
}

void TlsTcpConnectedClient::Impl::do_handshake() {
    IO_LOG(m_loop, TRACE, "Doing handshake");

    //auto handshake_result = SSL_accept(m_ssl);
    auto handshake_result = SSL_do_handshake(m_ssl);

    int write_pending = BIO_pending(m_ssl_write_bio);
    int read_pending = BIO_pending(m_ssl_read_bio);
    IO_LOG(m_loop, TRACE, "write_pending:", write_pending);
    IO_LOG(m_loop, TRACE, "read_pending:", read_pending);

    if (handshake_result < 0) {
        auto error = SSL_get_error(m_ssl, handshake_result);

        if (error == SSL_ERROR_WANT_READ) {
            IO_LOG(m_loop, TRACE, "SSL_ERROR_WANT_READ");

            const std::size_t BUF_SIZE = 4096;
            std::shared_ptr<char> buf(new char[BUF_SIZE], [](const char* p) { delete[] p; });
            const auto size = BIO_read(m_ssl_write_bio, buf.get(), BUF_SIZE);

            IO_LOG(m_loop, TRACE, "Getting data from SSL and sending to server, size:", size);
            m_tcp_client->send_data(buf, size);
        } else if (error == SSL_ERROR_WANT_WRITE) {
            IO_LOG(m_loop, TRACE, "SSL_ERROR_WANT_WRITE");
        } else {
            //char buf[4096];
            //ERR_error_string_n(SSL_get_error(m_ssl, error), buf, 4096);
            //IO_LOG(m_loop, ERROR, buf);

            char msg[1024];
            ERR_error_string_n(ERR_get_error(), msg, sizeof(msg));
            printf("%s %s %s %s\n", msg, ERR_lib_error_string(0), ERR_func_error_string(0), ERR_reason_error_string(0));

            // TODO: error handling here
        }
    } else if (handshake_result == 1) {
        IO_LOG(m_loop, TRACE, "Connected!!!!");

        if (write_pending) {
            const std::size_t BUF_SIZE = 4096;
            std::shared_ptr<char> buf(new char[BUF_SIZE], [](const char* p) { delete[] p; });
            const auto size = BIO_read(m_ssl_write_bio, buf.get(), BUF_SIZE);

            IO_LOG(m_loop, TRACE, "Getting data from SSL and sending to server, size:", size);
            m_tcp_client->send_data(buf, size);
        }

        m_ssl_handshake_complete = true;

        if (m_new_connection_callback) {
            m_new_connection_callback(*m_tls_server, *m_parent);
        }

        //if (m_connect_callback) {
        //    m_connect_callback(*this->m_parent, Error(0));
        //}
    } else {
        IO_LOG(m_loop, ERROR, "The TLS/SSL handshake was not successful but was shut down controlled and by the specifications of the TLS/SSL protocol.");
    }
}

/*
void TlsTcpConnectedClient::Impl::read_from_ssl() {
    IO_LOG(m_loop, TRACE, "Reading decrypted");
    const std::size_t SIZE = 16*1024; // https://www.openssl.org/docs/man1.0.2/man3/SSL_read.html
    std::unique_ptr<char[]> decrypted_buf(new char[SIZE]);

    int decrypted_size = SSL_read(m_ssl, decrypted_buf.get(), SIZE);
    while (decrypted_size > 0) {
        IO_LOG(m_loop, TRACE, "Decrypted message size:", decrypted_size);

        if (m_data_receive_callback) {
            m_data_receive_callback(*m_tls_server, *m_parent, decrypted_buf.get(), decrypted_size);
        }

        decrypted_size = SSL_read(m_ssl, decrypted_buf.get(), SIZE);
    }

    if (decrypted_size < 0) {
        int code = SSL_get_error(m_ssl, decrypted_size);
        if (code != SSL_ERROR_WANT_READ) {
            IO_LOG(m_loop, ERROR, "Failed to write buf of size", code);
            // TODO: handle error
            return;
        }
    }
}
*/

void TlsTcpConnectedClient::Impl::on_data_receive(const char* buf, std::size_t size) {
    // TODO: SSL_is_init_finished

    IO_LOG(m_loop, TRACE, "on_data_receive");

    if (m_ssl_handshake_complete) {
        const auto written_size = BIO_write(m_ssl_read_bio, buf, size);
        if (written_size < 0) {
            IO_LOG(m_loop, ERROR, "BIO_write failed with code:", written_size);
            return;
        }

        read_from_ssl();
    } else {
        const auto write_size = BIO_write(m_ssl_read_bio, buf, size);
        do_handshake();
    }
}

/*
bool TlsTcpConnectedClient::Impl::init_ssl() {
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
    m_ssl_ctx = SSL_CTX_new(TLSv1_2_server_method());
    if (m_ssl_ctx == nullptr) {
        IO_LOG(m_loop, ERROR, "Failed to init SSL context");
        return false;
    }

    // TODO: remove ???
    //SSL_CTX_set_ecdh_auto(m_ssl_ctx, 1);

    // TODO: remove this
    SSL_CTX_set_verify(m_ssl_ctx, SSL_VERIFY_NONE, NULL);

    result = SSL_CTX_set_cipher_list(m_ssl_ctx, "ALL:!SHA256:!SHA384:!aPSK:!ECDSA+SHA1:!ADH:!LOW:!EXP:!MD5");
    if (result == 0) {
        IO_LOG(m_loop, ERROR, "Failed to set siphers list");
        return false;
    }

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

    SSL_set_accept_state(m_ssl);

    IO_LOG(m_loop, DEBUG, "SSL inited");

    return true;
}
*/
void TlsTcpConnectedClient::Impl::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback) {
    const auto write_result = SSL_write(m_ssl, buffer.get(), size);
    if (write_result <= 0) {
        IO_LOG(m_loop, ERROR, "Failed to write buf of size", size);
        // TODO: handle error
        return;
    }

    // TODO: fixme
    const std::size_t SIZE = 1024 + size * 2; // TODO:
    std::shared_ptr<char> ptr(new char[SIZE], [](const char* p) { delete[] p;});

    const auto actual_size = BIO_read(m_ssl_write_bio, ptr.get(), SIZE);
    if (actual_size < 0) {
        IO_LOG(m_loop, ERROR, "BIO_read failed code:", actual_size);
        return;
    }

    IO_LOG(m_loop, TRACE, "Sending message to client. Original size:", size, "encrypted_size:", actual_size);
    m_tcp_client->send_data(ptr, actual_size, [callback, this](TcpConnectedClient& tcp_client, const Error& error) {
        if (callback) {
            callback(*m_parent, error);
        }
    });
}

void TlsTcpConnectedClient::Impl::send_data(const std::string& message, EndSendCallback callback) {
    std::shared_ptr<char> ptr(new char[message.size()], [](const char* p) { delete[] p;});
    std::copy(message.c_str(), message.c_str() + message.size(), ptr.get());
    send_data(ptr, static_cast<std::uint32_t>(message.size()), callback);
}

void TlsTcpConnectedClient::Impl::close() {
    m_tcp_client->close();
}

void TlsTcpConnectedClient::Impl::shutdown() {
    m_tcp_client->shutdown();
}

bool TlsTcpConnectedClient::Impl::is_open() const {
    return m_tcp_client->is_open();
}


///////////////////////////////////////// implementation ///////////////////////////////////////////

TlsTcpConnectedClient::TlsTcpConnectedClient(EventLoop& loop, TlsTcpServer& tls_server, NewConnectionCallback new_connection_callback, X509* certificate, EVP_PKEY* private_key, TcpConnectedClient& tcp_client) :
    Disposable(loop),
    m_impl(new Impl(loop, tls_server, new_connection_callback, certificate, private_key, tcp_client, *this)) {
}

TlsTcpConnectedClient::~TlsTcpConnectedClient() {
}

void TlsTcpConnectedClient::set_data_receive_callback(DataReceiveCallback callback) {
    return m_impl->set_data_receive_callback(callback);
}

void TlsTcpConnectedClient::on_data_receive(const char* buf, std::size_t size) {
    return m_impl->on_data_receive(buf, size);
}

bool TlsTcpConnectedClient::init_ssl() {
    return m_impl->ssl_init();
}

void TlsTcpConnectedClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback) {
    return m_impl->send_data(buffer, size, callback);
}

void TlsTcpConnectedClient::send_data(const std::string& message, EndSendCallback callback) {
    return m_impl->send_data(message, callback);
}

void TlsTcpConnectedClient::close() {
    return m_impl->close();
}

void TlsTcpConnectedClient::shutdown() {
    return m_impl->shutdown();
}

bool TlsTcpConnectedClient::is_open() const {
    return m_impl->is_open();
}


} // namespace io


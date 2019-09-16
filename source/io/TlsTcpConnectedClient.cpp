#include "TlsTcpConnectedClient.h"

#include "Common.h"
#include "TcpConnectedClient.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace io {

class TlsTcpConnectedClient::Impl {
public:
    Impl(EventLoop& loop, TlsTcpServer& tls_server, TcpConnectedClient& tcp_client, TlsTcpConnectedClient& parent);
    ~Impl();

    void close();
    void shutdown();
    bool is_open() const;

    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback);
    void send_data(const std::string& message, EndSendCallback callback);

    void set_data_receive_callback(DataReceiveCallback callback);
    void on_data_receive(const char* buf, std::size_t size);

    bool init_ssl();
    void do_handshake();

private:
    TlsTcpConnectedClient* m_parent;
    EventLoop* m_loop;
    TcpConnectedClient* m_tcp_client;
    TlsTcpServer* m_tls_server;

    DataReceiveCallback m_data_receive_callback = nullptr;

    bool m_ssl_handshake_complete = false;

    SSL_CTX* m_ssl_ctx = nullptr;
    BIO* m_ssl_read_bio = nullptr;
    BIO* m_ssl_write_bio = nullptr;
    SSL* m_ssl = nullptr;
};

TlsTcpConnectedClient::Impl::Impl(EventLoop& loop, TlsTcpServer& tls_server, TcpConnectedClient& tcp_client, TlsTcpConnectedClient& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_tcp_client(&tcp_client),
    m_tls_server(&tls_server) {
    m_tcp_client->set_user_data(&parent);
}

TlsTcpConnectedClient::Impl::~Impl() {
}

void TlsTcpConnectedClient::Impl::set_data_receive_callback(DataReceiveCallback callback) {
    m_data_receive_callback = callback;

    // TODO: move to some other place like callback which checks for accepting connection
    bool inited = init_ssl();
    if (inited) {
        SSL_set_accept_state(m_ssl);
    }
}

void TlsTcpConnectedClient::Impl::do_handshake() {
    IO_LOG(m_loop, TRACE, "Doing handshake");

    auto handshake_result = SSL_accept(m_ssl);
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
        m_ssl_handshake_complete = true;

        //if (m_connect_callback) {
        //    m_connect_callback(*this->m_parent, Error(0));
        //}
    } else {
        IO_LOG(m_loop, ERROR, "The TLS/SSL handshake was not successful but was shut down controlled and by the specifications of the TLS/SSL protocol.");
    }
}

void TlsTcpConnectedClient::Impl::on_data_receive(const char* buf, std::size_t size) {
    // TODO: SSL_is_init_finished

    IO_LOG(m_loop, TRACE, "on_data_receive");

    if (m_ssl_handshake_complete) {
        const auto written_size = BIO_write(m_ssl_read_bio, buf, size);
        if (written_size < 0) {
            IO_LOG(m_loop, ERROR, "BIO_write failed with code:", written_size);
            return;
        }

        IO_LOG(m_loop, TRACE, "Reading decrypted");
        const std::size_t SIZE = 16*1024; // https://www.openssl.org/docs/man1.0.2/man3/SSL_read.html
        std::unique_ptr<char[]> decrypted_buf(new char[SIZE]);

        int decrypted_size = SSL_read(m_ssl, decrypted_buf.get(), SIZE);
        while (decrypted_size > 0) {
            IO_LOG(m_loop, TRACE, "Decrypted message size:", decrypted_size, "original_size:", size);

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


    } else {
        const auto write_size = BIO_write(m_ssl_read_bio, buf, size);
        do_handshake();
    }
}

bool TlsTcpConnectedClient::Impl::init_ssl() {
    int result = 0;
    /*
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    int result = SSL_library_init();
    */
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    //if (!result) {
    //    IO_LOG(m_loop, ERROR, "Failed to init OpenSSL");
    //    return false;
    //}

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

    // TODO: hardcoded values!!!
    // TODO: reading for every client is overhead
    std::string cert_name = "certificate.pem";
    std::string key_name = "key.pem";

    result = SSL_CTX_use_certificate_file(m_ssl_ctx, cert_name.c_str(), SSL_FILETYPE_PEM);
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to load certificate", cert_name);
        return false;
    }

    result = SSL_CTX_use_PrivateKey_file(m_ssl_ctx, key_name.c_str(), SSL_FILETYPE_PEM);
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to load private key", key_name);
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

    result = SSL_CTX_check_private_key(m_ssl_ctx);
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to check private key");
        return false;
    }

    IO_LOG(m_loop, DEBUG, "SSL inited");

    return true;
}

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

    IO_LOG(m_loop, TRACE, "sending message to server of size:", actual_size);
    m_tcp_client->send_data(ptr, actual_size);
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

TlsTcpConnectedClient::TlsTcpConnectedClient(EventLoop& loop, TlsTcpServer& tls_server, TcpConnectedClient& tcp_client) :
    Disposable(loop),
    m_impl(new Impl(loop, tls_server, tcp_client, *this)) {
}

TlsTcpConnectedClient::~TlsTcpConnectedClient() {
}

void TlsTcpConnectedClient::set_data_receive_callback(DataReceiveCallback callback) {
    return m_impl->set_data_receive_callback(callback);
}

void TlsTcpConnectedClient::on_data_receive(const char* buf, std::size_t size) {
    return m_impl->on_data_receive(buf, size);
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


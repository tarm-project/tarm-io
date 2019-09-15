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
            char buf[4096];
            ERR_error_string_n(SSL_get_error(m_ssl, error), buf, 4096);
            IO_LOG(m_loop, ERROR, buf);
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

    if (m_ssl_handshake_complete) {
        if (m_data_receive_callback) {
            m_data_receive_callback(*m_tls_server, *m_parent, buf, size);
        }
    } else {
        const auto write_size = BIO_write(m_ssl_read_bio, buf, size);
        do_handshake();
    }
}

bool TlsTcpConnectedClient::Impl::init_ssl() {
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    int result = SSL_library_init();
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to init OpenSSL");
        return false;
    }

    // TODO: support different versions of TLS
    //m_ssl_ctx = SSL_CTX_new(TLSv1_2_server_method());
    m_ssl_ctx = SSL_CTX_new(TLSv1_2_server_method());
    if (m_ssl_ctx == nullptr) {
        IO_LOG(m_loop, ERROR, "Failed to init SSL context");
        return false;
    }

    // TODO: remove this
    SSL_CTX_set_verify(m_ssl_ctx, SSL_VERIFY_NONE, NULL);

    /*
    result = SSL_CTX_set_cipher_list(m_ssl_ctx, "ALL:!SHA256:!SHA384:!aPSK:!ECDSA+SHA1:!ADH:!LOW:!EXP:!MD5");
    if (result == 0) {
        IO_LOG(m_loop, ERROR, "Failed to set siphers list");
        return false;
    }*/

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

    result = SSL_CTX_check_private_key(m_ssl_ctx);
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to check private key");
        return false;
    }

    IO_LOG(m_loop, DEBUG, "SSL inited");

    return true;
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

} // namespace io


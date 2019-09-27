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

    //void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback);
    //void send_data(const std::string& message, EndSendCallback callback);

    void set_data_receive_callback(DataReceiveCallback callback);
    void on_data_receive(const char* buf, std::size_t size);

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

    void on_handshake_complete() override {
        if (m_new_connection_callback) {
            m_new_connection_callback(*m_tls_server, *m_parent);
        }
    }

private:
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

void TlsTcpConnectedClient::Impl::on_data_receive(const char* buf, std::size_t size) {
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

*/

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


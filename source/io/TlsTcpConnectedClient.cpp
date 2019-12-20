#include "TlsTcpConnectedClient.h"

#include "Common.h"
#include "TcpConnectedClient.h"
#include "detail/OpenSslClientImplBase.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace io {

class TlsTcpConnectedClient::Impl : public detail::OpenSslClientImplBase<TlsTcpConnectedClient, TlsTcpConnectedClient::Impl> {
public:
    Impl(EventLoop& loop, TlsTcpServer& tls_server, NewConnectionCallback new_connection_callback, X509* certificate, X509* private_key, TcpConnectedClient& tcp_client, TlsTcpConnectedClient& parent);
    ~Impl();

    void close();
    void shutdown();

    void set_data_receive_callback(DataReceiveCallback callback);

    TlsTcpServer& server();
    const TlsTcpServer& server() const;

protected:
    const SSL_METHOD* ssl_method() override;
    bool ssl_set_siphers() override;
    bool ssl_init_certificate_and_key() override;
    void ssl_set_state() override;

    void on_ssl_read(const DataChunk& data) override;
    void on_handshake_complete() override;

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
    OpenSslClientImplBase(loop, parent),
    m_tls_server(&tls_server),
    m_certificate(reinterpret_cast<::X509*>(certificate)),
    m_private_key(reinterpret_cast<::EVP_PKEY*>(private_key)),
    m_new_connection_callback(new_connection_callback) {
    m_client = &tcp_client;
    m_client->set_user_data(&parent);
}

TlsTcpConnectedClient::Impl::~Impl() {
}

void TlsTcpConnectedClient::Impl::set_data_receive_callback(DataReceiveCallback callback) {
    m_data_receive_callback = callback;
}

void TlsTcpConnectedClient::Impl::close() {
    m_client->close();
}

void TlsTcpConnectedClient::Impl::shutdown() {
    m_client->shutdown();
}

const SSL_METHOD* TlsTcpConnectedClient::Impl::ssl_method() {
    return TLSv1_2_server_method();
}

bool TlsTcpConnectedClient::Impl::ssl_set_siphers() {
    auto result = SSL_CTX_set_cipher_list(m_ssl_ctx, "ALL:!SHA256:!SHA384:!aPSK:!ECDSA+SHA1:!ADH:!LOW:!EXP:!MD5");
    if (result == 0) {
        IO_LOG(m_loop, ERROR, "Failed to set siphers list");
        return false;
    }
    return true;
}

bool TlsTcpConnectedClient::Impl::ssl_init_certificate_and_key() {
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

void TlsTcpConnectedClient::Impl::ssl_set_state() {
    SSL_set_accept_state(m_ssl);
}

void TlsTcpConnectedClient::Impl::on_ssl_read(const DataChunk& data) {
    if (m_data_receive_callback) {
        m_data_receive_callback(*m_parent, data, Error(0));
    }
}

void TlsTcpConnectedClient::Impl::on_handshake_complete() {
    if (m_new_connection_callback) {
        m_new_connection_callback(*m_parent, Error(0));
    }
}

TlsTcpServer& TlsTcpConnectedClient::Impl::server() {
    return *m_tls_server;
}

const TlsTcpServer& TlsTcpConnectedClient::Impl::server() const {
    return *m_tls_server;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TlsTcpConnectedClient::TlsTcpConnectedClient(EventLoop& loop, TlsTcpServer& tls_server, NewConnectionCallback new_connection_callback, X509* certificate, EVP_PKEY* private_key, TcpConnectedClient& tcp_client) :
    Removable(loop),
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

TlsTcpServer& TlsTcpConnectedClient::server() {
    return m_impl->server();
}

const TlsTcpServer& TlsTcpConnectedClient::server() const {
    return m_impl->server();
}

} // namespace io


#include "DtlsConnectedClient.h"

#include "Common.h"
#include "UdpPeer.h"
#include "detail/TlsTcpClientImplBase.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace io {

class DtlsConnectedClient::Impl : public detail::TlsTcpClientImplBase<DtlsConnectedClient, DtlsConnectedClient::Impl> {
public:
    Impl(EventLoop& loop, DtlsServer& dtls_server, NewConnectionCallback new_connection_callback, X509* certificate, X509* private_key, UdpPeer& udp_client, DtlsConnectedClient& parent);
    ~Impl();

    void close();
    void shutdown();

    void set_data_receive_callback(DataReceiveCallback callback);

protected:
    const SSL_METHOD* ssl_method() override;
    bool ssl_set_siphers() override;
    bool ssl_init_certificate_and_key() override;
    void ssl_set_state() override;

    void on_ssl_read(const char* buf, std::size_t size) override;
    void on_handshake_complete() override;

private:
    DtlsServer* m_dtls_server;

    ::X509* m_certificate;
    ::EVP_PKEY* m_private_key;

    DataReceiveCallback m_data_receive_callback = nullptr;
    NewConnectionCallback m_new_connection_callback = nullptr;
};

DtlsConnectedClient::Impl::Impl(EventLoop& loop,
                                DtlsServer& dtls_server,
                                NewConnectionCallback new_connection_callback,
                                X509* certificate,
                                EVP_PKEY* private_key,
                                UdpPeer& udp_client,
                                DtlsConnectedClient& parent) :
    TlsTcpClientImplBase(loop, parent),
    m_dtls_server(&dtls_server),
    m_certificate(reinterpret_cast<::X509*>(certificate)),
    m_private_key(reinterpret_cast<::EVP_PKEY*>(private_key)),
    m_new_connection_callback(new_connection_callback) {
    m_client = &udp_client;
    m_client->set_user_data(&parent);
}

DtlsConnectedClient::Impl::~Impl() {
}

void DtlsConnectedClient::Impl::set_data_receive_callback(DataReceiveCallback callback) {
    m_data_receive_callback = callback;
}

void DtlsConnectedClient::Impl::close() {
    // TODO: fixme
    //m_client->close();
}

void DtlsConnectedClient::Impl::shutdown() {
    // TODO: fixme
    //m_client->shutdown();
}

const SSL_METHOD* DtlsConnectedClient::Impl::ssl_method() {
    return DTLS_server_method();
}

bool DtlsConnectedClient::Impl::ssl_set_siphers() {
    // TODO: hardcoded ciphers list. Need to extract
    auto result = SSL_CTX_set_cipher_list(m_ssl_ctx, "ALL:!SHA256:!SHA384:!aPSK:!ECDSA+SHA1:!ADH:!LOW:!EXP:!MD5");
    if (result == 0) {
        IO_LOG(m_loop, ERROR, "Failed to set siphers list");
        return false;
    }
    return true;
}

bool DtlsConnectedClient::Impl::ssl_init_certificate_and_key() {
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

void DtlsConnectedClient::Impl::ssl_set_state() {
    SSL_set_accept_state(m_ssl);
}

void DtlsConnectedClient::Impl::on_ssl_read(const char* buf, std::size_t size) {
    if (m_data_receive_callback) {
        m_data_receive_callback(*m_dtls_server, *m_parent, buf, size);
    }
}

void DtlsConnectedClient::Impl::on_handshake_complete() {
    if (m_new_connection_callback) {
        m_new_connection_callback(*m_dtls_server, *m_parent);
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

DtlsConnectedClient::DtlsConnectedClient(EventLoop& loop, DtlsServer& dtls_server, NewConnectionCallback new_connection_callback, X509* certificate, EVP_PKEY* private_key, UdpPeer& udp_client) :
    Disposable(loop),
    m_impl(new Impl(loop, dtls_server, new_connection_callback, certificate, private_key, udp_client, *this)) {
}

DtlsConnectedClient::~DtlsConnectedClient() {
}

void DtlsConnectedClient::set_data_receive_callback(DataReceiveCallback callback) {
    return m_impl->set_data_receive_callback(callback);
}

void DtlsConnectedClient::on_data_receive(const char* buf, std::size_t size) {
    return m_impl->on_data_receive(buf, size);
}

bool DtlsConnectedClient::init_ssl() {
    return m_impl->ssl_init();
}

void DtlsConnectedClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback) {
    return m_impl->send_data(buffer, size, callback);
}

void DtlsConnectedClient::send_data(const std::string& message, EndSendCallback callback) {
    return m_impl->send_data(message, callback);
}

void DtlsConnectedClient::close() {
    return m_impl->close();
}

void DtlsConnectedClient::shutdown() {
    return m_impl->shutdown();
}

bool DtlsConnectedClient::is_open() const {
    return m_impl->is_open();
}


} // namespace io

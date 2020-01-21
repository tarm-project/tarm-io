#include "DtlsConnectedClient.h"

#include "Common.h"
#include "UdpPeer.h"
#include "detail/DtlsContext.h"
#include "detail/OpenSslClientImplBase.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace io {

class DtlsConnectedClient::Impl : public detail::OpenSslClientImplBase<DtlsConnectedClient, DtlsConnectedClient::Impl> {
public:
    Impl(EventLoop& loop, DtlsServer& dtls_server, NewConnectionCallback new_connection_callback, UdpPeer& udp_client, const detail::DtlsContext& context, DtlsConnectedClient& parent);
    ~Impl();

    void close();
    void shutdown();

    void set_data_receive_callback(DataReceiveCallback callback);

    DtlsServer& server();
    const DtlsServer& server() const;

protected:
    const SSL_METHOD* ssl_method() override;
    bool ssl_set_siphers() override;
    void ssl_set_versions() override;
    bool ssl_init_certificate_and_key() override;
    void ssl_set_state() override;

    void on_ssl_read(const DataChunk& data) override;
    void on_handshake_complete() override;
    void on_handshake_failed(long openssl_error_code, const Error& error) override;

private:
    DtlsServer* m_dtls_server;

    detail::DtlsContext m_dtls_context;

    DataReceiveCallback m_data_receive_callback = nullptr;
    NewConnectionCallback m_new_connection_callback = nullptr;
};

DtlsConnectedClient::Impl::Impl(EventLoop& loop,
                                DtlsServer& dtls_server,
                                NewConnectionCallback new_connection_callback,
                                UdpPeer& udp_client,
                                const detail::DtlsContext& context,
                                DtlsConnectedClient& parent) :
    OpenSslClientImplBase(loop, parent),
    m_dtls_server(&dtls_server),
    m_dtls_context(context),
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

DtlsServer& DtlsConnectedClient::Impl::server() {
    return *m_dtls_server;
}

const DtlsServer& DtlsConnectedClient::Impl::server() const {
    return *m_dtls_server;
}

const SSL_METHOD* DtlsConnectedClient::Impl::ssl_method() {
// OpenSSL before version 1.0.2 had no generic method for DTLS and only supported DTLS 1.0
#if OPENSSL_VERSION_NUMBER < 0x1000200fL
    return DTLSv1_server_method();
#else
    return DTLS_server_method();
#endif
}

bool DtlsConnectedClient::Impl::ssl_set_siphers() {
    // TODO: hardcoded ciphers list. Need to extract
    auto result = SSL_CTX_set_cipher_list(this->ssl_ctx(), "ALL:!SHA256:!SHA384:!aPSK:!ECDSA+SHA1:!ADH:!LOW:!EXP:!MD5");
    if (result == 0) {
        IO_LOG(m_loop, ERROR, "Failed to set siphers list");
        return false;
    }
    return true;
}

void DtlsConnectedClient::Impl::ssl_set_versions() {
    this->set_dtls_version(std::get<0>(m_dtls_context.dtls_version_range), std::get<1>(m_dtls_context.dtls_version_range));
}

bool DtlsConnectedClient::Impl::ssl_init_certificate_and_key() {
    auto result = SSL_CTX_use_certificate(this->ssl_ctx(), m_dtls_context.certificate);
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to load certificate");
        return false;
    }

    result = SSL_CTX_use_PrivateKey(this->ssl_ctx(), m_dtls_context.private_key);
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to load private key");
        return false;
    }

    result = SSL_CTX_check_private_key(this->ssl_ctx());
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to check private key");
        return false;
    }

    return true;
}

void DtlsConnectedClient::Impl::ssl_set_state() {
    SSL_set_accept_state(this->ssl());
}

void DtlsConnectedClient::Impl::on_ssl_read(const DataChunk& data) {
    if (m_data_receive_callback) {
        m_data_receive_callback(*m_parent, data, Error(0));
    }
}

void DtlsConnectedClient::Impl::on_handshake_complete() {
    if (m_new_connection_callback) {
        m_new_connection_callback(*m_parent, Error(0));
    }
}

void DtlsConnectedClient::Impl::on_handshake_failed(long /*openssl_error_code*/, const Error& error) {
    if (m_new_connection_callback) {
        m_new_connection_callback(*m_parent, error);
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

DtlsConnectedClient::DtlsConnectedClient(EventLoop& loop, DtlsServer& dtls_server, NewConnectionCallback new_connection_callback, UdpPeer& udp_client, void* context) :
    Removable(loop),
    m_impl(new Impl(loop, dtls_server, new_connection_callback, udp_client, *reinterpret_cast<detail::DtlsContext*>(context), *this)) {
}

DtlsConnectedClient::~DtlsConnectedClient() {
}

void DtlsConnectedClient::set_data_receive_callback(DataReceiveCallback callback) {
    return m_impl->set_data_receive_callback(callback);
}

void DtlsConnectedClient::on_data_receive(const char* buf, std::size_t size) {
    return m_impl->on_data_receive(buf, size);
}

Error DtlsConnectedClient::init_ssl() {
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

DtlsVersion DtlsConnectedClient::negotiated_dtls_version() const {
    return m_impl->negotiated_dtls_version();
}

DtlsServer& DtlsConnectedClient::server() {
    return m_impl->server();
}

const DtlsServer& DtlsConnectedClient::server() const {
    return m_impl->server();
}

} // namespace io

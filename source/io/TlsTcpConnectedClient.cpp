#include "TlsTcpConnectedClient.h"

#include "Convert.h"
#include "TcpConnectedClient.h"
#include "TlsTcpServer.h"
#include "detail/ConstexprString.h"
#include "detail/OpenSslClientImplBase.h"
#include "detail/TlsContext.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace io {

class TlsTcpConnectedClient::Impl : public detail::OpenSslClientImplBase<TlsTcpConnectedClient, TlsTcpConnectedClient::Impl> {
public:
    Impl(EventLoop& loop,
         TlsTcpServer& tls_server,
         NewConnectionCallback new_connection_callback,
         TcpConnectedClient& tcp_client,
         const detail::TlsContext& context,
         TlsTcpConnectedClient& parent);
    ~Impl();

    Error init_ssl();

    void close();
    void shutdown();

    void set_data_receive_callback(DataReceiveCallback callback);

    TlsTcpServer& server();
    const TlsTcpServer& server() const;

protected:
    void ssl_set_state() override;

    void on_ssl_read(const DataChunk& data, const Error& error) override;
    void on_handshake_complete() override;
    void on_handshake_failed(long openssl_error_code, const Error& error) override;

private:
    TlsTcpServer* m_tls_server = nullptr;;

    detail::TlsContext m_tls_context;

    DataReceiveCallback m_data_receive_callback = nullptr;
    NewConnectionCallback m_new_connection_callback = nullptr;
};

TlsTcpConnectedClient::Impl::Impl(EventLoop& loop,
                                  TlsTcpServer& tls_server,
                                  NewConnectionCallback new_connection_callback,
                                  TcpConnectedClient& tcp_client,
                                  const detail::TlsContext& context,
                                  TlsTcpConnectedClient& parent) :
    OpenSslClientImplBase(loop, parent),
    m_tls_server(&tls_server),
    m_tls_context(context),
    m_new_connection_callback(new_connection_callback) {
    m_client = &tcp_client;
    m_client->set_user_data(&parent);
}

TlsTcpConnectedClient::Impl::~Impl() {
}

Error TlsTcpConnectedClient::Impl::init_ssl() {
    return ssl_init(m_tls_context.ssl_ctx);
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
/*
const SSL_METHOD* TlsTcpConnectedClient::Impl::ssl_method() {
    return SSLv23_server_method(); // This call includes also TLS versions
}

void TlsTcpConnectedClient::Impl::ssl_set_versions() {
    this->set_tls_version(std::get<0>(m_tls_context.tls_version_range), std::get<1>(m_tls_context.tls_version_range));
}

bool TlsTcpConnectedClient::Impl::ssl_init_certificate_and_key() {
    auto result = SSL_CTX_use_certificate(this->ssl_ctx(), m_tls_context.certificate);
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to load certificate");
        return false;
    }

    result = SSL_CTX_use_PrivateKey(this->ssl_ctx(), m_tls_context.private_key);
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
*/
void TlsTcpConnectedClient::Impl::ssl_set_state() {
    SSL_set_accept_state(this->ssl());
}

void TlsTcpConnectedClient::Impl::on_ssl_read(const DataChunk& data, const Error& error) {
    if (m_data_receive_callback) {
        m_data_receive_callback(*m_parent, data, error);
    }
}

void TlsTcpConnectedClient::Impl::on_handshake_complete() {
    if (m_new_connection_callback) {
        m_new_connection_callback(*m_parent, Error(0));
    }
}

void TlsTcpConnectedClient::Impl::on_handshake_failed(long openssl_error_code, const Error& error) {
    if ((openssl_error_code & 0xFFF) == SSL_R_UNKNOWN_PROTOCOL) {
        // Sending version failed alert manually because OpensSSL does not do it.
        const std::size_t BUF_SIZE = 7;
        std::shared_ptr<char> buf_ptr(new char[BUF_SIZE], std::default_delete<char[]>());
        buf_ptr.get()[0] = 0x15;
        buf_ptr.get()[1] = 0x03;
        buf_ptr.get()[2] = 0x01; // TLS 1.0 by default
        buf_ptr.get()[3] = 0x00;
        buf_ptr.get()[4] = 0x02;
        buf_ptr.get()[5] = 0x02;
        buf_ptr.get()[6] = 0x46;

        //char buf[] = {0x15, 0x03, 0x01, 0x00, 0x02, 0x02, 0x46};
        if (std::get<1>(m_tls_server->version_range()) != TlsVersion::UNKNOWN) {
            buf_ptr.get()[2] = static_cast<unsigned char>(std::get<1>(m_tls_server->version_range()));
        }

        m_client->send_data(buf_ptr, BUF_SIZE);
    }

    if (m_new_connection_callback) {
        m_new_connection_callback(*this->m_parent, error);
    }

    m_client->close();
}

TlsTcpServer& TlsTcpConnectedClient::Impl::server() {
    return *m_tls_server;
}

const TlsTcpServer& TlsTcpConnectedClient::Impl::server() const {
    return *m_tls_server;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TlsTcpConnectedClient::TlsTcpConnectedClient(EventLoop& loop,
                                             TlsTcpServer& tls_server,
                                             NewConnectionCallback new_connection_callback,
                                             TcpConnectedClient& tcp_client,
                                             void* context) :
    Removable(loop),
    m_impl(new Impl(loop, tls_server, new_connection_callback, tcp_client, *reinterpret_cast<detail::TlsContext*>(context), *this)) {
}

TlsTcpConnectedClient::~TlsTcpConnectedClient() {
}

void TlsTcpConnectedClient::set_data_receive_callback(DataReceiveCallback callback) {
    return m_impl->set_data_receive_callback(callback);
}

void TlsTcpConnectedClient::on_data_receive(const char* buf, std::size_t size) {
    return m_impl->on_data_receive(buf, size);
}

Error TlsTcpConnectedClient::init_ssl() {
    return m_impl->init_ssl();
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

TlsVersion TlsTcpConnectedClient::negotiated_tls_version() const {
    return m_impl->negotiated_tls_version();
}

} // namespace io


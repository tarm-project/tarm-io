/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "TlsTcpConnectedClient.h"

#include "Convert.h"
#include "TcpConnectedClient.h"
#include "TlsTcpServer.h"
#include "detail/ConstexprString.h"
#include "detail/OpenSslClientImplBase.h"
#include "detail/TlsContext.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace tarm {
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
    void on_alert(int code) override;

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
    if (m_new_connection_callback) {
        m_new_connection_callback(*this->m_parent, error);
    }

    m_client->close();
}

void TlsTcpConnectedClient::Impl::on_alert(int code) {
    // Do nothing
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
} // namespace tarm


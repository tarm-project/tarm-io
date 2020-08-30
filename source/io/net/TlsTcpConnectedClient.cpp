/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "net/TlsTcpConnectedClient.h"

#include "Convert.h"
#include "net/TcpConnectedClient.h"
#include "net/TlsTcpServer.h"
#include "detail/ConstexprString.h"
#include "detail/OpenSslClientImplBase.h"
#include "detail/TlsContext.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace tarm {
namespace io {
namespace net {

class TlsConnectedClient::Impl : public detail::OpenSslClientImplBase<TlsConnectedClient, TlsConnectedClient::Impl> {
public:
    Impl(EventLoop& loop,
         TlsServer& tls_server,
         const NewConnectionCallback& new_connection_callback,
         TcpConnectedClient& tcp_client,
         const detail::TlsContext& context,
         TlsConnectedClient& parent);
    ~Impl();

    Error init_ssl();

    void close();
    void shutdown();

    void set_data_receive_callback(const DataReceiveCallback& callback);
    void on_data_receive(const char* buf, std::size_t size, const Error& error);

    TlsServer& server();
    const TlsServer& server() const;

protected:
    void ssl_set_state() override;

    void on_ssl_read(const DataChunk& data, const Error& error) override;
    void on_handshake_complete() override;
    void on_handshake_failed(long openssl_error_code, const Error& error) override;
    void on_alert(int code) override;

private:
    TlsServer* m_tls_server = nullptr;;

    detail::TlsContext m_tls_context;

    DataReceiveCallback m_data_receive_callback = nullptr;
    NewConnectionCallback m_new_connection_callback = nullptr;
};

TlsConnectedClient::Impl::Impl(EventLoop& loop,
                                  TlsServer& tls_server,
                                  const NewConnectionCallback& new_connection_callback,
                                  TcpConnectedClient& tcp_client,
                                  const detail::TlsContext& context,
                                  TlsConnectedClient& parent) :
    OpenSslClientImplBase(loop, parent),
    m_tls_server(&tls_server),
    m_tls_context(context),
    m_new_connection_callback(new_connection_callback) {
    m_client = &tcp_client;
    m_client->set_user_data(&parent);
}

TlsConnectedClient::Impl::~Impl() {
}

Error TlsConnectedClient::Impl::init_ssl() {
    return ssl_init(m_tls_context.ssl_ctx);
}

void TlsConnectedClient::Impl::set_data_receive_callback(const DataReceiveCallback& callback) {
    m_data_receive_callback = callback;
}

void TlsConnectedClient::Impl::on_data_receive(const char* buf, std::size_t size, const Error& error) {
    if (error) {
        if (m_data_receive_callback) {
            m_data_receive_callback(*m_parent, {nullptr, 0}, error);
        }
    } else {
        detail::OpenSslClientImplBase<TlsConnectedClient, TlsConnectedClient::Impl>::on_data_receive(buf, size);
    }
}

void TlsConnectedClient::Impl::close() {
    m_client->close();
}

void TlsConnectedClient::Impl::shutdown() {
    m_client->shutdown();
}

void TlsConnectedClient::Impl::ssl_set_state() {
    SSL_set_accept_state(this->ssl());
}

void TlsConnectedClient::Impl::on_ssl_read(const DataChunk& data, const Error& error) {
    if (m_data_receive_callback) {
        m_data_receive_callback(*m_parent, data, error);
    }
}

void TlsConnectedClient::Impl::on_handshake_complete() {
    if (m_new_connection_callback) {
        m_new_connection_callback(*m_parent, Error(0));
    }
}

void TlsConnectedClient::Impl::on_handshake_failed(long openssl_error_code, const Error& error) {
    // TODO: need to investigate this. Looks like sometimes alert is sent
    //       it may depend on timings or (likely) OpenSSL version.
    const auto ssl_fail_reason = ERR_GET_REASON(openssl_error_code);
    if (ssl_fail_reason == SSL_R_UNKNOWN_PROTOCOL) {
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

void TlsConnectedClient::Impl::on_alert(int code) {
    // Do nothing
}

TlsServer& TlsConnectedClient::Impl::server() {
    return *m_tls_server;
}

const TlsServer& TlsConnectedClient::Impl::server() const {
    return *m_tls_server;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TlsConnectedClient::TlsConnectedClient(EventLoop& loop,
                                       TlsServer& tls_server,
                                       const NewConnectionCallback& new_connection_callback,
                                       TcpConnectedClient& tcp_client,
                                       void* context) :
    Removable(loop),
    m_impl(new Impl(loop, tls_server, new_connection_callback, tcp_client, *reinterpret_cast<detail::TlsContext*>(context), *this)) {
}

TlsConnectedClient::~TlsConnectedClient() {
}

void TlsConnectedClient::set_data_receive_callback(const DataReceiveCallback& callback) {
    return m_impl->set_data_receive_callback(callback);
}

void TlsConnectedClient::on_data_receive(const char* buf, std::size_t size, const Error& error) {
    return m_impl->on_data_receive(buf, size, error);
}

Error TlsConnectedClient::init_ssl() {
    return m_impl->init_ssl();
}

void TlsConnectedClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback) {
    return m_impl->send_data(buffer, size, callback);
}

void TlsConnectedClient::send_data(const std::string& message, const EndSendCallback& callback) {
    return m_impl->send_data(message, callback);
}

void TlsConnectedClient::send_data(std::string&& message, const EndSendCallback& callback) {
    return m_impl->send_data(std::move(message), callback);
}

void TlsConnectedClient::send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback) {
    return m_impl->send_data(c_str, size, callback);
}

void TlsConnectedClient::close() {
    return m_impl->close();
}

void TlsConnectedClient::shutdown() {
    return m_impl->shutdown();
}

bool TlsConnectedClient::is_open() const {
    return m_impl->is_open();
}

TlsServer& TlsConnectedClient::server() {
    return m_impl->server();
}

const TlsServer& TlsConnectedClient::server() const {
    return m_impl->server();
}

TlsVersion TlsConnectedClient::negotiated_tls_version() const {
    return m_impl->negotiated_tls_version();
}

} // namespace net
} // namespace io
} // namespace tarm


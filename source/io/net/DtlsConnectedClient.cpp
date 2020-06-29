/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "net/DtlsConnectedClient.h"

#include "Convert.h"
#include "net/DtlsServer.h"
#include "net/UdpPeer.h"
#include "detail/net/DtlsContext.h"
#include "detail/ConstexprString.h"
#include "detail/net/OpenSslClientImplBase.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace tarm {
namespace io {
namespace net {

class DtlsConnectedClient::Impl : public detail::OpenSslClientImplBase<DtlsConnectedClient, DtlsConnectedClient::Impl> {
public:
    // Constructor for temporary DtlsConnectedClient objects just to signal an error
    Impl(EventLoop& loop,
         DtlsServer& dtls_server,
         UdpPeer& udp_peer,
         DtlsConnectedClient& parent);

    Impl(EventLoop& loop,
         DtlsServer& dtls_server,
         const NewConnectionCallback& new_connection_callback,
         const CloseCallback& close_callback,
         UdpPeer& udp_client,
         const detail::DtlsContext& context,
         DtlsConnectedClient& parent);
    ~Impl();

    Error init_ssl();

    void close();

    void set_data_receive_callback(const DataReceiveCallback& callback);

    DtlsServer& server();
    const DtlsServer& server() const;

protected:
    void ssl_set_state() override;

    void on_ssl_read(const DataChunk& data, const Error& error) override;
    void on_handshake_complete() override;
    void on_handshake_failed(long openssl_error_code, const Error& error) override;
    void on_alert(int code) override;

private:
    DtlsServer* m_dtls_server;

    detail::DtlsContext m_dtls_context;

    DataReceiveCallback m_data_receive_callback = nullptr;
    NewConnectionCallback m_new_connection_callback = nullptr;
    CloseCallback m_close_callback = nullptr;
};

DtlsConnectedClient::Impl::Impl(EventLoop& loop,
                                DtlsServer& dtls_server,
                                UdpPeer& udp_peer,
                                DtlsConnectedClient& parent) :
    OpenSslClientImplBase(loop, parent),
    m_dtls_server(&dtls_server) {
    m_client = &udp_peer;
}

DtlsConnectedClient::Impl::Impl(EventLoop& loop,
                                DtlsServer& dtls_server,
                                const NewConnectionCallback& new_connection_callback,
                                const CloseCallback& close_callback,
                                UdpPeer& udp_client,
                                const detail::DtlsContext& context,
                                DtlsConnectedClient& parent) :
    OpenSslClientImplBase(loop, parent),
    m_dtls_server(&dtls_server),
    m_dtls_context(context),
    m_new_connection_callback(new_connection_callback),
    m_close_callback(close_callback) {
    m_client = &udp_client;
    m_client->set_user_data(&parent);

    IO_LOG(m_loop, TRACE, m_parent, "New DtlsConnectedClient");
}

DtlsConnectedClient::Impl::~Impl() {
    IO_LOG(m_loop, TRACE, m_parent, "Delete DtlsConnectedClient");
}

Error DtlsConnectedClient::Impl::init_ssl() {
    return ssl_init(m_dtls_context.ssl_ctx);
}

void DtlsConnectedClient::Impl::set_data_receive_callback(const DataReceiveCallback& callback) {
    m_data_receive_callback = callback;
}

void DtlsConnectedClient::Impl::close() {
    if (!this->is_open()) {
        return;
    }

    const auto error = this->ssl_shutdown([this](UdpPeer& client, const Error& error) {
        if (m_close_callback) {
            m_close_callback(*m_parent, Error(0));
        }
        m_client->close();
    });

    m_dtls_server->remove_client(*m_parent);

    if (error) {
        if (m_close_callback) {
            m_close_callback(*m_parent, error);
        }
    }
}

DtlsServer& DtlsConnectedClient::Impl::server() {
    return *m_dtls_server;
}

const DtlsServer& DtlsConnectedClient::Impl::server() const {
    return *m_dtls_server;
}

void DtlsConnectedClient::Impl::ssl_set_state() {
    SSL_set_accept_state(this->ssl());
}

void DtlsConnectedClient::Impl::on_ssl_read(const DataChunk& data, const Error& error) {
    if (m_data_receive_callback) {
        m_data_receive_callback(*m_parent, data, error);
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

void DtlsConnectedClient::Impl::on_alert(int code) {
    IO_LOG(m_loop, DEBUG, m_parent, "");

    if (code == SSL3_AD_CLOSE_NOTIFY) {
        m_dtls_server->remove_client(*m_parent);

        if (m_close_callback) {
            m_close_callback(*m_parent, Error(0));
        }
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

DtlsConnectedClient::DtlsConnectedClient(EventLoop& loop,
                                         DtlsServer& dtls_server,
                                         const NewConnectionCallback& new_connection_callback,
                                         const CloseCallback& close_callback,
                                         UdpPeer& udp_peer,
                                         void* context) :
    Removable(loop),
    m_impl(new Impl(loop, dtls_server, new_connection_callback, close_callback, udp_peer, *reinterpret_cast<detail::DtlsContext*>(context), *this)) {
}

DtlsConnectedClient::DtlsConnectedClient(EventLoop& loop,
                                         DtlsServer& dtls_server,
                                         UdpPeer& udp_peer) :
    Removable(loop),
    m_impl(new Impl(loop, dtls_server, udp_peer, *this)) {
}

DtlsConnectedClient::~DtlsConnectedClient() {
}

void DtlsConnectedClient::set_data_receive_callback(const DataReceiveCallback& callback) {
    return m_impl->set_data_receive_callback(callback);
}

void DtlsConnectedClient::on_data_receive(const char* buf, std::size_t size) {
    return m_impl->on_data_receive(buf, size);
}

Error DtlsConnectedClient::init_ssl() {
    return m_impl->init_ssl();
}

void DtlsConnectedClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback) {
    return m_impl->send_data(buffer, size, callback);
}

void DtlsConnectedClient::send_data(const std::string& message, const EndSendCallback& callback) {
    return m_impl->send_data(message, callback);
}

void DtlsConnectedClient::send_data(std::string&& message, const EndSendCallback& callback) {
    return m_impl->send_data(std::move(message), callback);
}

void DtlsConnectedClient::send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback) {
    return m_impl->send_data(c_str, size, callback);
}

void DtlsConnectedClient::close() {
    return m_impl->close();
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

const Endpoint& DtlsConnectedClient::endpoint() const {
    return m_impl->endpoint();
}

} // namespace net
} // namespace io
} // namespace tarm

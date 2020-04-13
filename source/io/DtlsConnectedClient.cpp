#include "DtlsConnectedClient.h"

#include "Convert.h"
#include "UdpPeer.h"
#include "detail/DtlsContext.h"
#include "detail/ConstexprString.h"
#include "detail/OpenSslClientImplBase.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace io {

class DtlsConnectedClient::Impl : public detail::OpenSslClientImplBase<DtlsConnectedClient, DtlsConnectedClient::Impl> {
public:
    Impl(EventLoop& loop,
         DtlsServer& dtls_server,
         NewConnectionCallback new_connection_callback,
         CloseCallback close_callback,
         UdpPeer& udp_client,
         const detail::DtlsContext& context,
         DtlsConnectedClient& parent);
    ~Impl();

    Error init_ssl();

    void close();

    void set_data_receive_callback(DataReceiveCallback callback);

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
                                NewConnectionCallback new_connection_callback,
                                CloseCallback close_callback,
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
}

DtlsConnectedClient::Impl::~Impl() {
}

Error DtlsConnectedClient::Impl::init_ssl() {
    return ssl_init(m_dtls_context.ssl_ctx);
}

void DtlsConnectedClient::Impl::set_data_receive_callback(DataReceiveCallback callback) {
    m_data_receive_callback = callback;
}

void DtlsConnectedClient::Impl::close() {
    const auto error = this->ssl_shutdown([this](io::UdpPeer& client, const io::Error& error) {
        if (m_close_callback) {
            m_close_callback(*m_parent, Error(0));
        }
        m_client->close(1000); // TODO: hardcode
    });

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
        if (m_close_callback) {
            m_close_callback(*m_parent, Error(0));
        }
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

DtlsConnectedClient::DtlsConnectedClient(EventLoop& loop,
                                         DtlsServer& dtls_server,
                                         NewConnectionCallback new_connection_callback,
                                         CloseCallback close_callback,
                                         UdpPeer& udp_client,
                                         void* context) :
    Removable(loop),
    m_impl(new Impl(loop, dtls_server, new_connection_callback, close_callback, udp_client, *reinterpret_cast<detail::DtlsContext*>(context), *this)) {
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
    return m_impl->init_ssl();
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

} // namespace io

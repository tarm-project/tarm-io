#include "DtlsClient.h"

#include "ByteSwap.h"
#include "Convert.h"
#include "UdpClient.h"
#include "detail/ConstexprString.h"
#include "detail/OpenSslClientImplBase.h"
#include "detail/OpenSslContext.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <string>

namespace io {

class DtlsClient::Impl : public detail::OpenSslClientImplBase<DtlsClient, DtlsClient::Impl> {
public:
    Impl(EventLoop& loop, DtlsVersionRange version_range, DtlsClient& parent);
    ~Impl();

    std::uint32_t ipv4_addr() const;
    std::uint16_t port() const;

    void connect(const Endpoint& endpoint,
                 ConnectCallback connect_callback,
                 DataReceiveCallback receive_callback,
                 CloseCallback close_callback);
    void close();

protected:
    void ssl_set_state() override;
    const SSL_METHOD* ssl_method();

    void on_ssl_read(const DataChunk& data, const Error& error) override;
    void on_handshake_complete() override;
    void on_handshake_failed(long openssl_error_code, const Error& error) override;

private:
    ConnectCallback m_connect_callback;
    DataReceiveCallback m_receive_callback;
    CloseCallback m_close_callback;
    DtlsVersionRange m_version_range;

    detail::OpenSslContext<DtlsClient, DtlsClient::Impl> m_openssl_context;
};

DtlsClient::Impl::~Impl() {
}

DtlsClient::Impl::Impl(EventLoop& loop, DtlsVersionRange version_range, DtlsClient& parent) :
    OpenSslClientImplBase(loop, parent),
    m_version_range(version_range),
    m_openssl_context(loop, parent) {
}

std::uint32_t DtlsClient::Impl::ipv4_addr() const {
    return m_client->ipv4_addr();
}

std::uint16_t DtlsClient::Impl::port() const {
    return m_client->port();
}

void DtlsClient::Impl::connect(const Endpoint& endpoint,
                               ConnectCallback connect_callback,
                               DataReceiveCallback receive_callback,
                               CloseCallback close_callback) {
    if (!is_ssl_inited()) {
        auto context_errror = m_openssl_context.init_ssl_context(ssl_method());
        if (context_errror) {
            m_loop->schedule_callback([=]() { connect_callback(*this->m_parent, context_errror); });
            return;
        }
        m_openssl_context.set_dtls_version(std::get<0>(m_version_range), std::get<1>(m_version_range));

        Error ssl_init_error = ssl_init(m_openssl_context.ssl_ctx());
        if (ssl_init_error) {
            m_loop->schedule_callback([=]() { connect_callback(*this->m_parent, ssl_init_error); });
            return;
        }
    }

    m_client = new UdpClient(*m_loop,
                             endpoint,
                             [this](UdpClient&, const DataChunk& chunk, const Error&) {
                                 this->on_data_receive(chunk.buf.get(), chunk.size);
                             }
    );

    m_connect_callback = connect_callback;
    m_receive_callback = receive_callback;
    m_close_callback = close_callback;

    do_handshake();
}

void DtlsClient::Impl::close() {
    // TODO: implement
}

const SSL_METHOD* DtlsClient::Impl::ssl_method() {
// OpenSSL before version 1.0.2 had no generic method for DTLS and only supported DTLS 1.0
#if OPENSSL_VERSION_NUMBER < 0x1000200fL
    return DTLSv1_client_method();
#else
    return DTLS_client_method();
#endif
}
/*
void DtlsClient::Impl::ssl_set_versions() {
    this->set_dtls_version(std::get<0>(m_version_range), std::get<1>(m_version_range));
}
*/
void DtlsClient::Impl::ssl_set_state() {
    SSL_set_connect_state(this->ssl());
}

void DtlsClient::Impl::on_ssl_read(const DataChunk& data, const Error& error) {
    if (m_receive_callback) {
        m_receive_callback(*this->m_parent, data, error);
    }
}

void DtlsClient::Impl::on_handshake_complete() {
    if (m_connect_callback) {
        m_connect_callback(*this->m_parent, Error(0));
    }
}

void DtlsClient::Impl::on_handshake_failed(long /*openssl_error_code*/, const Error& error) {
    if (m_connect_callback) {
        m_connect_callback(*this->m_parent, error);
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////



DtlsClient::DtlsClient(EventLoop& loop, DtlsVersionRange version_range) :
    Removable(loop),
    m_impl(new Impl(loop, version_range, *this)) {
}

DtlsClient::~DtlsClient() {
}

void DtlsClient::schedule_removal() {
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

std::uint32_t DtlsClient::ipv4_addr() const {
    return m_impl->ipv4_addr();
}

std::uint16_t DtlsClient::port() const {
    return m_impl->port();
}

void DtlsClient::connect(const Endpoint& endpoint,
                         ConnectCallback connect_callback,
                         DataReceiveCallback receive_callback,
                         CloseCallback close_callback) {
    return m_impl->connect(endpoint, connect_callback, receive_callback, close_callback);
}

void DtlsClient::close() {
    return m_impl->close();
}

bool DtlsClient::is_open() const {
    return m_impl->is_open();
}

void DtlsClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback) {
    return m_impl->send_data(buffer, size, callback);
}

void DtlsClient::send_data(const std::string& message, EndSendCallback callback) {
    return m_impl->send_data(message, callback);
}

DtlsVersion DtlsClient::negotiated_dtls_version() const {
    return m_impl->negotiated_dtls_version();
}

} // namespace io

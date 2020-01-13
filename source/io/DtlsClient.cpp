#include "DtlsClient.h"

#include "ByteSwap.h"
#include "Common.h"
#include "UdpClient.h"
#include "detail/OpenSslClientImplBase.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <string>

// TODO: improve log messages here

namespace io {

class DtlsClient::Impl : public detail::OpenSslClientImplBase<DtlsClient, DtlsClient::Impl> {
public:
    Impl(EventLoop& loop, DtlsClient& parent);
    ~Impl();

    std::uint32_t ipv4_addr() const;
    std::uint16_t port() const;

    void connect(const std::string& address,
                 std::uint16_t port,
                 ConnectCallback connect_callback,
                 DataReceiveCallback receive_callback,
                 CloseCallback close_callback);
    void close();

protected:
    const SSL_METHOD* ssl_method() override;
    bool ssl_set_siphers() override;
    void ssl_set_versions() override;
    bool ssl_init_certificate_and_key() override;
    void ssl_set_state() override;

    void on_ssl_read(const DataChunk& data) override;
    void on_handshake_complete() override;

private:
    ConnectCallback m_connect_callback;
    DataReceiveCallback m_receive_callback;
    CloseCallback m_close_callback;
};

DtlsClient::Impl::~Impl() {
}

DtlsClient::Impl::Impl(EventLoop& loop, DtlsClient& parent) :
    OpenSslClientImplBase(loop, parent) {
}

std::uint32_t DtlsClient::Impl::ipv4_addr() const {
    return m_client->ipv4_addr();
}

std::uint16_t DtlsClient::Impl::port() const {
    return m_client->port();
}

void DtlsClient::Impl::connect(const std::string& address,
                 std::uint16_t port,
                 ConnectCallback connect_callback,
                 DataReceiveCallback receive_callback,
                 CloseCallback close_callback) {

    struct sockaddr_in addr;
    uv_ip4_addr(address.c_str(), port, &addr); // TODO: error handling

    m_client = new UdpClient(*m_loop,
                             network_to_host(addr.sin_addr.s_addr),
                             port,
                             [this](UdpClient&, const DataChunk& chunk, const Error&) {
        this->on_data_receive(chunk.buf.get(), chunk.size);
    });

    if (!is_ssl_inited()) {
        // TODO: error handling
        Error ssl_init_error = ssl_init();
    }

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

bool DtlsClient::Impl::ssl_set_siphers() {
    auto result = SSL_CTX_set_cipher_list(this->ssl_ctx(), "ALL:!SHA256:!SHA384:!aPSK:!ECDSA+SHA1:!ADH:!LOW:!EXP:!MD5");
    if (result == 0) {
        IO_LOG(m_loop, ERROR, "Failed to set siphers list");
        return false;
    }
    return true;
}

void DtlsClient::Impl::ssl_set_versions() {
    // Do nothing for now
}

bool DtlsClient::Impl::ssl_init_certificate_and_key() {
    // Do nothing
    return true;
}

void DtlsClient::Impl::ssl_set_state() {
    SSL_set_connect_state(this->ssl());
}

void DtlsClient::Impl::on_ssl_read(const DataChunk& data) {
    if (m_receive_callback) {
        m_receive_callback(*this->m_parent, data, Error(0));
    }
}

void DtlsClient::Impl::on_handshake_complete() {
    if (m_connect_callback) {
        m_connect_callback(*this->m_parent, Error(0));
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

DtlsClient::DtlsClient(EventLoop& loop) :
    Removable(loop),
    m_impl(new Impl(loop, *this)) {
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

void DtlsClient::connect(const std::string& address,
                           std::uint16_t port,
                           ConnectCallback connect_callback,
                           DataReceiveCallback receive_callback,
                           CloseCallback close_callback) {
    return m_impl->connect(address, port, connect_callback, receive_callback, close_callback);
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

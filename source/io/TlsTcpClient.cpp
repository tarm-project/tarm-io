#include "TlsTcpClient.h"

#include "Common.h"
#include "TcpClient.h"
#include "detail/OpenSslClientImplBase.h"


#include <openssl/ssl.h>
#include <openssl/err.h>

#include <string>

// TODO: improve log messages here

namespace io {

class TlsTcpClient::Impl : public detail::TlsTcpClientImplBase<TlsTcpClient, TlsTcpClient::Impl> {
public:
    Impl(EventLoop& loop, TlsTcpClient& parent);
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
    bool ssl_init_certificate_and_key() override;
    void ssl_set_state() override;

    void on_ssl_read(const DataChunk& data) override;
    void on_handshake_complete() override;

private:
    ConnectCallback m_connect_callback;
    DataReceiveCallback m_receive_callback;
    CloseCallback m_close_callback;
};

TlsTcpClient::Impl::~Impl() {
}

TlsTcpClient::Impl::Impl(EventLoop& loop, TlsTcpClient& parent) :
    TlsTcpClientImplBase(loop, parent) {
}

std::uint32_t TlsTcpClient::Impl::ipv4_addr() const {
    return m_client->ipv4_addr();
}

std::uint16_t TlsTcpClient::Impl::port() const {
    return m_client->port();
}

void TlsTcpClient::Impl::connect(const std::string& address,
                 std::uint16_t port,
                 ConnectCallback connect_callback,
                 DataReceiveCallback receive_callback,
                 CloseCallback close_callback) {
    m_client = new TcpClient(*m_loop);

    // TODO: check is_connected
    bool is_connected = ssl_init();

    m_connect_callback = connect_callback;
    m_receive_callback = receive_callback;
    m_close_callback = close_callback;

    std::function<void(TcpClient&, const Error&)> on_connect =
        [this](TcpClient& client, const Error& error) {
            if (error) {
                m_connect_callback(*this->m_parent, error);
                return;
            }

            do_handshake();
        };

    std::function<void(TcpClient&, const DataChunk& data, const Error& error)> on_data_receive =
        [this](TcpClient& client, const DataChunk& data, const Error& error) {
            // TODO: error handling
            this->on_data_receive(data.buf.get(), data.size);
        };

    std::function<void(TcpClient&, const Error&)> on_close =
        [this](TcpClient& client, const Error& error) {
            IO_LOG(m_loop, TRACE, "Close", error.code());
            if (m_close_callback) {
                m_close_callback(*this->m_parent, error);
            }
        };

    m_client->connect(address, port, on_connect, on_data_receive, on_close);
}

void TlsTcpClient::Impl::close() {
    // TODO: implement
}

const SSL_METHOD* TlsTcpClient::Impl::ssl_method() {
    return TLSv1_2_client_method();
}

bool TlsTcpClient::Impl::ssl_set_siphers() {
    auto result = SSL_CTX_set_cipher_list(m_ssl_ctx, "ALL:!SHA256:!SHA384:!aPSK:!ECDSA+SHA1:!ADH:!LOW:!EXP:!MD5");
    if (result == 0) {
        IO_LOG(m_loop, ERROR, "Failed to set siphers list");
        return false;
    }
    return true;
}

bool TlsTcpClient::Impl::ssl_init_certificate_and_key() {
    // Do nothing
    return true;
}

void TlsTcpClient::Impl::ssl_set_state() {
    SSL_set_connect_state(m_ssl);
}

void TlsTcpClient::Impl::on_ssl_read(const DataChunk& data) {
    if (m_receive_callback) {
        m_receive_callback(*this->m_parent, data, Error(0));
    }
}

void TlsTcpClient::Impl::on_handshake_complete() {
    if (m_connect_callback) {
        m_connect_callback(*this->m_parent, Error(0));
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TlsTcpClient::TlsTcpClient(EventLoop& loop) :
    Removable(loop),
    m_impl(new Impl(loop, *this)) {
}

TlsTcpClient::~TlsTcpClient() {
}

void TlsTcpClient::schedule_removal() {
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

std::uint32_t TlsTcpClient::ipv4_addr() const {
    return m_impl->ipv4_addr();
}

std::uint16_t TlsTcpClient::port() const {
    return m_impl->port();
}

void TlsTcpClient::connect(const std::string& address,
                           std::uint16_t port,
                           ConnectCallback connect_callback,
                           DataReceiveCallback receive_callback,
                           CloseCallback close_callback) {
    return m_impl->connect(address, port, connect_callback, receive_callback, close_callback);
}

void TlsTcpClient::close() {
    return m_impl->close();
}

bool TlsTcpClient::is_open() const {
    return m_impl->is_open();
}

void TlsTcpClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback) {
    return m_impl->send_data(buffer, size, callback);
}

void TlsTcpClient::send_data(const std::string& message, EndSendCallback callback) {
    return m_impl->send_data(message, callback);
}

} // namespace io

#include "TlsTcpClient.h"

#include "Common.h"
#include "TcpClient.h"
#include "detail/TlsTcpClientImplBase.h"


#include <openssl/ssl.h>
#include <openssl/err.h>

#include <string>

// TODO: improve log messages here

namespace io {

class TlsTcpClient::Impl : public detail::TlsTcpClientImplBase<TlsTcpClient, TlsTcpClient::Impl> {
public:
    Impl(EventLoop& loop, TlsTcpClient& parent);
    ~Impl();

    bool schedule_removal();

    std::uint32_t ipv4_addr() const;
    std::uint16_t port() const;

    void connect(const std::string& address,
                 std::uint16_t port,
                 ConnectCallback connect_callback,
                 DataReceiveCallback receive_callback,
                 CloseCallback close_callback);
    void close();
    bool is_open() const;

protected:
    const SSL_METHOD* ssl_method() override;
    bool ssl_set_siphers() override;
    bool ssl_init_certificate_and_key() override;
    void ssl_set_state() override;

    void on_ssl_read(const char* buf, std::size_t size) override;
    void on_handshake_complete() override;

private:
    ConnectCallback m_connect_callback;
    DataReceiveCallback m_receive_callback;
    CloseCallback m_close_callback;

    // Removal is scheduled in 2 steps. First TCP connection is removed, then TLS
    bool m_ready_schedule_removal = false;
};

TlsTcpClient::Impl::~Impl() {
}

TlsTcpClient::Impl::Impl(EventLoop& loop, TlsTcpClient& parent) :
    TlsTcpClientImplBase(loop, parent) {
}

bool TlsTcpClient::Impl::schedule_removal() {
    IO_LOG(m_loop, TRACE, "");

    if (m_tcp_client) {
        if (!m_ready_schedule_removal) {
            m_tcp_client->schedule_removal();
            m_ready_schedule_removal = true;
            return false; // postpone removal
        }
    }

    return true;
}

std::uint32_t TlsTcpClient::Impl::ipv4_addr() const {
    return m_tcp_client->ipv4_addr();
}

std::uint16_t TlsTcpClient::Impl::port() const {
    return m_tcp_client->port();
}

void TlsTcpClient::Impl::connect(const std::string& address,
                 std::uint16_t port,
                 ConnectCallback connect_callback,
                 DataReceiveCallback receive_callback,
                 CloseCallback close_callback) {
    m_tcp_client = new TcpClient(*m_loop);

    bool is_connected = ssl_init();

    m_connect_callback = connect_callback;
    m_receive_callback = receive_callback;
    m_close_callback = close_callback;

    std::function<void(TcpClient&, const Error&)> on_connect =
        [this](TcpClient& client, const Error& error) {
            do_handshake();
        };

    std::function<void(TcpClient&, const char*, size_t)> on_data_receive =
        [this](TcpClient& client, const char* buf, size_t size) {
            this->on_data_receive(buf, size);
        };

    std::function<void(TcpClient&, const Error&)> on_close =
        [this](TcpClient& client, const Error& error) {
            IO_LOG(m_loop, TRACE, "Close", error.code());
            if (m_close_callback) {
                m_close_callback(*this->m_parent, error);
            }

            if (m_ready_schedule_removal) {
                m_parent->schedule_removal();
            }
        };

    m_tcp_client->connect(address, port, on_connect, on_data_receive, on_close);
}

void TlsTcpClient::Impl::close() {
    // TODO: implement
}

bool TlsTcpClient::Impl::is_open() const {
    // TODO: implement
    return true;
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

void TlsTcpClient::Impl::on_ssl_read(const char* buf, std::size_t size) {
    if (m_receive_callback) {
        m_receive_callback(*this->m_parent, buf, size);
    }
}

void TlsTcpClient::Impl::on_handshake_complete() {
    if (m_connect_callback) {
        m_connect_callback(*this->m_parent, Error(0));
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TlsTcpClient::TlsTcpClient(EventLoop& loop) :
    Disposable(loop),
    m_impl(new Impl(loop, *this)) {
}

TlsTcpClient::~TlsTcpClient() {
}

void TlsTcpClient::schedule_removal() {
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Disposable::schedule_removal();
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

#include "TlsTcpClient.h"

#include "Convert.h"
#include "TcpClient.h"
#include "detail/ConstexprString.h"
#include "detail/OpenSslClientImplBase.h"
#include "detail/OpenSslContext.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <string>

namespace io {

class TlsTcpClient::Impl : public detail::OpenSslClientImplBase<TlsTcpClient, TlsTcpClient::Impl> {
public:
    Impl(EventLoop& loop, TlsVersionRange version_range, TlsTcpClient& parent);
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
    const SSL_METHOD* ssl_method();
    void ssl_set_state() override;

    void on_ssl_read(const DataChunk& data, const Error& error) override;
    void on_handshake_complete() override;
    void on_handshake_failed(long openssl_error_code, const Error& error) override;

private:
    ConnectCallback m_connect_callback;
    DataReceiveCallback m_receive_callback;
    CloseCallback m_close_callback;
    TlsVersionRange m_version_range;

    detail::OpenSslContext<TlsTcpClient, TlsTcpClient::Impl> m_openssl_context;
};

TlsTcpClient::Impl::~Impl() {
}

TlsTcpClient::Impl::Impl(EventLoop& loop, TlsVersionRange version_range, TlsTcpClient& parent) :
    OpenSslClientImplBase(loop, parent),
    m_version_range(version_range),
    m_openssl_context(loop, parent) {
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

    if (!is_ssl_inited()) {
        auto context_errror = m_openssl_context.init_ssl_context(ssl_method());
        if (context_errror) {
            m_loop->schedule_callback([=]() { connect_callback(*this->m_parent, context_errror); });
            return;
        }

        auto version_error = m_openssl_context.set_tls_version(std::get<0>(m_version_range), std::get<1>(m_version_range));
        if (version_error) {
            m_loop->schedule_callback([=]() { connect_callback(*this->m_parent, version_error); });
            return;
        }

        Error ssl_init_error = this->ssl_init(m_openssl_context.ssl_ctx());
        if (ssl_init_error) {
            m_loop->schedule_callback([=]() { connect_callback(*this->m_parent, ssl_init_error); });
            return;
        }
    }

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
            IO_LOG(m_loop, TRACE, this->m_parent, "Close", error.code());

            if (!this->m_ssl_handshake_complete) {
                return;
            }

            /*
            // TODO: need to revise this. Is it possible to indicate version mismatch error by sending TLS fragment or so.
            if (!this->m_ssl_handshake_complete) {
                IO_LOG(m_loop, ERROR, this->m_parent, "Handshake failed");
                if (this->m_connect_callback) {
                    this->m_connect_callback(*this->m_parent, Error(StatusCode::OPENSSL_ERROR, "Handshake error"));
                }
                return;
            }
            */

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
    return SSLv23_client_method(); // This call includes also TLS versions
}
/*
void TlsTcpClient::Impl::ssl_set_versions() {
    this->set_tls_version(std::get<0>(m_version_range), std::get<1>(m_version_range));
}

bool TlsTcpClient::Impl::ssl_init_certificate_and_key() {
    // Do nothing
    return true;
}
*/
void TlsTcpClient::Impl::ssl_set_state() {
    SSL_set_connect_state(this->ssl());
}

void TlsTcpClient::Impl::on_ssl_read(const DataChunk& data, const Error& error) {
    if (m_receive_callback) {
        m_receive_callback(*this->m_parent, data, error);
    }
}

void TlsTcpClient::Impl::on_handshake_complete() {
    if (m_connect_callback) {
        m_connect_callback(*this->m_parent, Error(0));
    }
}

void TlsTcpClient::Impl::on_handshake_failed(long /*openssl_error_code*/, const Error& error) {
    if (m_connect_callback) {
        m_connect_callback(*this->m_parent, error);
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////



TlsTcpClient::TlsTcpClient(EventLoop& loop, TlsVersionRange version_range) :
    Removable(loop),
    m_impl(new Impl(loop, version_range, *this)) {
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

TlsVersion TlsTcpClient::negotiated_tls_version() const {
    return m_impl->negotiated_tls_version();
}

} // namespace io

/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "net/TlsClient.h"

#include "Convert.h"
#include "net/TcpClient.h"
#include "detail/ConstexprString.h"
#include "detail/OpenSslClientImplBase.h"
#include "detail/OpenSslContext.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <string>

namespace tarm {
namespace io {
namespace net {

class TlsClient::Impl : public detail::OpenSslClientImplBase<TlsClient, TlsClient::Impl> {
public:
    Impl(AllocationContext& context, TlsVersionRange version_range, TlsClient& parent);
    ~Impl();

    const Endpoint& endpoint() const;

    void connect(const Endpoint endpoint,
                 const ConnectCallback& connect_callback,
                 const DataReceiveCallback& receive_callback,
                 const CloseCallback& close_callback);
    void close();

protected:
    const SSL_METHOD* ssl_method();
    void ssl_set_state() override;

    void on_ssl_read(const DataChunk& data, const Error& error) override;
    void on_handshake_complete() override;
    void on_handshake_failed(long openssl_error_code, const Error& error) override;
    void on_alert(int code) override;

private:
    ConnectCallback m_connect_callback;
    DataReceiveCallback m_receive_callback;
    CloseCallback m_close_callback;
    TlsVersionRange m_version_range;

    detail::OpenSslContext<TlsClient, TlsClient::Impl> m_openssl_context;
};

TlsClient::Impl::~Impl() {
}

// TODO: init SSL in constructor???
TlsClient::Impl::Impl(AllocationContext& context, TlsVersionRange version_range, TlsClient& parent) :
    OpenSslClientImplBase(context.loop, parent),
    m_version_range(version_range),
    m_openssl_context(context.loop, parent) {
}

const Endpoint& TlsClient::Impl::endpoint() const {
    return m_client->endpoint();
}

void TlsClient::Impl::connect(const Endpoint endpoint,
                              const ConnectCallback& connect_callback,
                              const DataReceiveCallback& receive_callback,
                              const CloseCallback& close_callback) {
    // TODO: handle TcpClient creation error properly
    // TODO: m_client can leak here if call connect twice
    // TODO: likely move SSL init calls in constructor
    m_client = new TcpClient(*m_loop);

    if (!is_ssl_inited()) {
        auto context_errror = m_openssl_context.init_ssl_context(ssl_method());
        if (context_errror) {
            m_loop->schedule_callback([=](EventLoop&) { connect_callback(*this->m_parent, context_errror); });
            return;
        }

        auto version_error = m_openssl_context.set_tls_version(std::get<0>(m_version_range), std::get<1>(m_version_range));
        if (version_error) {
            m_loop->schedule_callback([=](EventLoop&) { connect_callback(*this->m_parent, version_error); });
            return;
        }

        Error ssl_init_error = this->ssl_init(m_openssl_context.ssl_ctx());
        if (ssl_init_error) {
            m_loop->schedule_callback([=](EventLoop&) { connect_callback(*this->m_parent, ssl_init_error); });
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
            if (error) {
                if (m_receive_callback) {
                    m_receive_callback(*m_parent, {nullptr, 0}, error);
                }
            } else {
                this->on_data_receive(data.buf.get(), data.size);
            }
        };

    std::function<void(TcpClient&, const Error&)> on_close =
        [this](TcpClient& client, const Error& error) {
            LOG_TRACE(m_loop, this->m_parent, "Close", error.code());

            if (this->m_ssl_handshake_state != HandshakeState::FINISHED) {
                return;
            }

            /*
            // TODO: need to revise this. Is it possible to indicate version mismatch error by sending TLS fragment or so.
            if (!this->m_ssl_handshake_complete) {
                LOG_ERROR(m_loop, this->m_parent, "Handshake failed");
                if (this->m_connect_callback) {
                    this->m_connect_callback(*this->m_parent, Error(StatusCode::OPENSSL_ERROR, "Handshake error"));
                }
                return;
            }
            */

            if (m_close_callback) {
                m_close_callback(*this->m_parent, error);
                m_close_callback = nullptr; // Not reacting on SSL shutdown callback if any
            }
        };

    m_client->connect(endpoint, on_connect, on_data_receive, on_close);
}

void TlsClient::Impl::close() {
    const auto error = this->ssl_shutdown([this](TcpClient& client, const Error& error) {
        if (m_close_callback) {
            m_close_callback(*m_parent, Error(0));
            m_close_callback = nullptr; // Not reacting on TCP close afterwards
        }
    });

    if (error) {
        if (m_close_callback) {
            m_close_callback(*m_parent, error);
        }
    }
}

const SSL_METHOD* TlsClient::Impl::ssl_method() {
    return SSLv23_client_method(); // This call includes also TLS versions
}

void TlsClient::Impl::ssl_set_state() {
    SSL_set_connect_state(this->ssl());
}

void TlsClient::Impl::on_ssl_read(const DataChunk& data, const Error& error) {
    if (m_receive_callback) {
        m_receive_callback(*this->m_parent, data, error);
    }
}

void TlsClient::Impl::on_handshake_complete() {
    if (m_connect_callback) {
        m_connect_callback(*this->m_parent, Error(0));
    }
}

void TlsClient::Impl::on_handshake_failed(long /*openssl_error_code*/, const Error& error) {
    if (m_connect_callback) {
        m_connect_callback(*this->m_parent, error);
    }
}

void TlsClient::Impl::on_alert(int /*code*/) {
    // Do nothing
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TlsClient::TlsClient(AllocationContext& context) :
    Removable(context.loop),
    m_impl(new Impl(context, DEFAULT_TLS_VERSION_RANGE, *this)) {
}

TlsClient::TlsClient(AllocationContext& context, TlsVersionRange version_range) :
    Removable(context.loop),
    m_impl(new Impl(context, version_range, *this)) {
}

TlsClient::~TlsClient() {
}

void TlsClient::schedule_removal() {
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

const Endpoint& TlsClient::endpoint() const {
    return m_impl->endpoint();
}

void TlsClient::connect(const Endpoint endpoint,
                           const ConnectCallback& connect_callback,
                           const DataReceiveCallback& receive_callback,
                           const CloseCallback& close_callback) {
    return m_impl->connect(endpoint, connect_callback, receive_callback, close_callback);
}

void TlsClient::close() {
    return m_impl->close();
}

bool TlsClient::is_open() const {
    return m_impl->is_open();
}

void TlsClient::copy_and_send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback) {
    return m_impl->copy_and_send_data(c_str, size, callback);
}

void TlsClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback) {
    return m_impl->send_data(buffer, size, callback);
}

void TlsClient::send_data(const std::string& message, const EndSendCallback& callback) {
    return m_impl->send_data(message, callback);
}

void TlsClient::send_data(std::string&& message, const EndSendCallback& callback) {
    return m_impl->send_data(std::move(message), callback);
}

void TlsClient::send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback) {
    return m_impl->send_data(c_str, size, callback);
}

TlsVersion TlsClient::negotiated_tls_version() const {
    return m_impl->negotiated_tls_version();
}

} // namespace net
} // namespace io
} // namespace tarm

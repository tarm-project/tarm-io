/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "net/DtlsClient.h"

#include "ByteSwap.h"
#include "Convert.h"
#include "net/UdpClient.h"
#include "detail/ConstexprString.h"
#include "detail/OpenSslClientImplBase.h"
#include "detail/OpenSslContext.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <string>

namespace tarm {
namespace io {
namespace net {

class DtlsClient::Impl : public detail::OpenSslClientImplBase<DtlsClient, DtlsClient::Impl> {
public:
    Impl(AllocationContext& context, DtlsVersionRange version_range, DtlsClient& parent);
    ~Impl();

    void connect(const Endpoint& endpoint,
                 const ConnectCallback& connect_callback,
                 const DataReceiveCallback& receive_callback,
                 const CloseCallback& close_callback,
                 std::size_t timeout_ms);
    void close();

    std::uint16_t bound_port() const;

protected:
    void ssl_set_state() override;
    const SSL_METHOD* ssl_method();

    void on_ssl_read(const DataChunk& data, const Error& error) override;
    void on_handshake_complete() override;
    void on_handshake_failed(long openssl_error_code, const Error& error) override;
    void on_alert(int code) override;

private:
    ConnectCallback m_connect_callback;
    DataReceiveCallback m_receive_callback;
    CloseCallback m_close_callback;
    DtlsVersionRange m_version_range;

    detail::OpenSslContext<DtlsClient, DtlsClient::Impl> m_openssl_context;
};

DtlsClient::Impl::~Impl() {
    LOG_TRACE(m_loop, m_parent, "Deleted DtlsClient");
}

DtlsClient::Impl::Impl(AllocationContext& context, DtlsVersionRange version_range, DtlsClient& parent) :
    OpenSslClientImplBase(context.loop, parent),
    m_version_range(version_range),
    m_openssl_context(context.loop, parent) { // TODO: pass error to allocation context on fail to init OpenSSL?
    LOG_TRACE(m_loop, m_parent, "New DtlsClient");
}

void DtlsClient::Impl::connect(const Endpoint& endpoint,
                               const ConnectCallback& connect_callback,
                               const DataReceiveCallback& receive_callback,
                               const CloseCallback& close_callback,
                               std::size_t timeout_ms) {
    if (endpoint.type() == Endpoint::UNDEFINED) {
        if (connect_callback) {
            m_loop->schedule_callback([=](EventLoop&) {
                connect_callback(*m_parent, Error(StatusCode::INVALID_ARGUMENT));
            });
        }
        return;
    }

    m_client = new UdpClient(*m_loop);
    m_client->set_destination(endpoint,
        [this, connect_callback](UdpClient&, const Error& error) {
            if (error) {
                if (connect_callback) {
                    connect_callback(*this->m_parent, error);
                }
                return;
            }

            if (!is_ssl_inited()) {
                auto context_error = m_openssl_context.init_ssl_context(ssl_method());
                if (context_error) {
                    m_loop->schedule_callback([=](EventLoop&) { connect_callback(*this->m_parent, context_error); });
                    return;
                }

                auto version_error = m_openssl_context.set_dtls_version(std::get<0>(m_version_range),
                                                                        std::get<1>(m_version_range));
                if (version_error) {
                    m_loop->schedule_callback([=](EventLoop&) { connect_callback(*this->m_parent, version_error); });
                    return;
                }

                Error ssl_init_error = ssl_init(m_openssl_context.ssl_ctx());
                if (ssl_init_error) {
                    m_loop->schedule_callback([=](EventLoop&) { connect_callback(*this->m_parent, ssl_init_error); });
                    return;
                }
            }

            do_handshake();
        },
        [this](UdpClient&, const DataChunk& chunk, const Error& error) {
            if (error) {
                if (m_receive_callback) {
                    m_receive_callback(*m_parent, {nullptr, 0}, error);
                }
            } else {
                on_data_receive(chunk.buf.get(), chunk.size);
            }
        },
        timeout_ms,
        [this](UdpClient&, const Error& error) {
            if (m_ssl_handshake_state != HandshakeState::FINISHED) {
                if (m_connect_callback) {
                    m_connect_callback(*m_parent, StatusCode::CONNECTION_REFUSED);
                    return;
                }
            }

            if (m_close_callback) {
                m_close_callback(*m_parent, error);
            }
        }
    );

    m_connect_callback = connect_callback;
    m_receive_callback = receive_callback;
    m_close_callback = close_callback;
}

void DtlsClient::Impl::close() {
    const auto error = this->ssl_shutdown([this](UdpClient& client, const Error& error) {
        m_client->close();
    });

    if (error) {
        if (m_close_callback) {
            m_close_callback(*m_parent, error);
        }
    }
}

const SSL_METHOD* DtlsClient::Impl::ssl_method() {
// OpenSSL before version 1.0.2 had no generic method for DTLS and only supported DTLS 1.0
#if OPENSSL_VERSION_NUMBER < 0x1000200fL
    return DTLSv1_client_method();
#else
    return DTLS_client_method();
#endif
}

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

void DtlsClient::Impl::on_alert(int code) {
    LOG_DEBUG(m_loop, m_parent, "code:", code);

    if (code == SSL3_AD_CLOSE_NOTIFY) {
        if (m_close_callback) {
            m_close_callback(*m_parent, Error(0));
        }
    }
}

std::uint16_t DtlsClient::Impl::bound_port() const {
    return m_client ? m_client->bound_port() : 0;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////


DtlsClient::DtlsClient(AllocationContext& context, DtlsVersionRange version_range) :
    Removable(context.loop),
    m_impl(new Impl(context, version_range, *this)) {
}

DtlsClient::DtlsClient(AllocationContext& context) :
    DtlsClient(context, DEFAULT_DTLS_VERSION_RANGE) {
}



DtlsClient::~DtlsClient() {
}

void DtlsClient::schedule_removal() {
    const bool ready_to_remove = m_impl->schedule_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

const Endpoint& DtlsClient::endpoint() const {
    return m_impl->endpoint();
}

void DtlsClient::connect(const Endpoint& endpoint,
                         const ConnectCallback& connect_callback,
                         const DataReceiveCallback& receive_callback,
                         const CloseCallback& close_callback,
                         std::size_t timeout_ms) {
    return m_impl->connect(endpoint, connect_callback, receive_callback, close_callback, timeout_ms);
}

void DtlsClient::close() {
    return m_impl->close();
}

bool DtlsClient::is_open() const {
    return m_impl->is_open();
}

void DtlsClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, const EndSendCallback& callback) {
    return m_impl->send_data(buffer, size, callback);
}

void DtlsClient::send_data(const std::string& message, const EndSendCallback& callback) {
    return m_impl->send_data(message, callback);
}

void DtlsClient::send_data(std::string&& message, const EndSendCallback& callback) {
    return m_impl->send_data(std::move(message), callback);
}

void DtlsClient::send_data(const char* c_str, std::uint32_t size, const EndSendCallback& callback) {
    return m_impl->send_data(c_str, size, callback);
}

DtlsVersion DtlsClient::negotiated_dtls_version() const {
    return m_impl->negotiated_dtls_version();
}

std::uint16_t DtlsClient::bound_port() const {
    return m_impl->bound_port();
}

} // namespace net
} // namespace io
} // namespace tarm

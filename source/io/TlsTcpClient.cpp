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

    //void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback);
    //void send_data(const std::string& message, EndSendCallback callback);

protected:
    const SSL_METHOD* ssl_method() override {
        return TLSv1_2_client_method();
    }

    bool ssl_set_siphers() override {
        auto result = SSL_CTX_set_cipher_list(m_ssl_ctx, "ALL:!SHA256:!SHA384:!aPSK:!ECDSA+SHA1:!ADH:!LOW:!EXP:!MD5");
        if (result == 0) {
            IO_LOG(m_loop, ERROR, "Failed to set siphers list");
            return false;
        }
        return true;
    }

    bool ssl_init_certificate_and_key() override {
        // Do nothing
        return true;
    }

    void ssl_set_state() override {
        SSL_set_connect_state(m_ssl);
    }

    void on_ssl_read(const char* buf, std::size_t size) override {
        if (m_receive_callback) {
            m_receive_callback(*this->m_parent, buf, size);
        }
    }

    void on_handshake_complete() override {
        if (m_connect_callback) {
            m_connect_callback(*this->m_parent, Error(0));
        }
    }

private:
    ConnectCallback m_connect_callback;
    DataReceiveCallback m_receive_callback;
    CloseCallback m_close_callback;
};

TlsTcpClient::Impl::~Impl() {
}

TlsTcpClient::Impl::Impl(EventLoop& loop, TlsTcpClient& parent) :
    TlsTcpClientImplBase(loop, parent) {
    m_tcp_client = new TcpClient(loop);
}

bool TlsTcpClient::Impl::schedule_removal() {
    IO_LOG(m_loop, TRACE, "");

    // TODO: need to revise this?????
    m_tcp_client->schedule_removal();
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
            IO_LOG(m_loop, TRACE, "Received data from server, size: ", size);

            if (m_ssl_handshake_complete) {
                const auto written_size = BIO_write(m_ssl_read_bio, buf, size);
                if (written_size < 0) {
                    IO_LOG(m_loop, ERROR, "BIO_write failed with code:", written_size);
                    return;
                }

                read_from_ssl();
            } else {
                auto write_result = BIO_write(m_ssl_read_bio, buf, size);
                IO_LOG(m_loop, TRACE, "BIO_write result:", write_result);
                // TODO: error handling here

                do_handshake();
            }
        };

    std::function<void(TcpClient&, const Error&)> on_close =
        [this](TcpClient& client, const Error& error) {
            IO_LOG(m_loop, TRACE, "Close", error.code());
            if (m_close_callback) {
                m_close_callback(*this->m_parent, error);
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
/*
void TlsTcpClient::Impl::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback) {
    const auto write_result = SSL_write(m_ssl, buffer.get(), size);
    if (write_result <= 0) {
        IO_LOG(m_loop, ERROR, "Failed to write buf of size", size);
        // TODO: handle error
        return;
    }

    // TODO: fixme
    const std::size_t SIZE = 1024 + size * 2; // TODO:
    std::shared_ptr<char> ptr(new char[SIZE], [](const char* p) { delete[] p;});

    const auto actual_size = BIO_read(m_ssl_write_bio, ptr.get(), SIZE);
    if (actual_size < 0) {
        IO_LOG(m_loop, ERROR, "BIO_read failed code:", actual_size);
        return;
    }

    IO_LOG(m_loop, TRACE, "sending message to server of size:", actual_size);
    m_tcp_client->send_data(ptr, actual_size, [callback, this](TcpClient& client, const Error& error) {
        if (callback) {
            callback(*m_parent, error);
        }
    });
}

void TlsTcpClient::Impl::send_data(const std::string& message, EndSendCallback callback) {
    std::shared_ptr<char> ptr(new char[message.size()], [](const char* p) { delete[] p;});
    std::copy(message.c_str(), message.c_str() + message.size(), ptr.get());
    send_data(ptr, static_cast<std::uint32_t>(message.size()), callback);
}

*/

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

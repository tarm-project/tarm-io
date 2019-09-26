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

    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback);
    void send_data(const std::string& message, EndSendCallback callback);

protected:
    //bool init_ssl();

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

    //void read_from_ssl();

    void on_ssl_read(const char* buf, std::size_t size) override {
        if (m_receive_callback) {
            m_receive_callback(*this->m_parent, buf, size);
        }
    }

    void do_handshake();

private:
    TlsTcpClient* m_parent;
    EventLoop* m_loop;

    TcpClient* m_tcp_client;

    ConnectCallback m_connect_callback;
    DataReceiveCallback m_receive_callback;
    CloseCallback m_close_callback;

    std::function<void(TcpClient&, const char*, size_t)> m_on_data_receive = nullptr;
};

TlsTcpClient::Impl::~Impl() {
}
/*
bool TlsTcpClient::Impl::init_ssl() {
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    // TODO: potantially heavy code in 'SSL_library_init'. Need to investigate impact
    int result = SSL_library_init();
    if (!result) {
        IO_LOG(m_loop, ERROR, "Failed to init OpenSSL");
        return false;
    }

    // TODO: support different versions of TLS
    m_ssl_ctx = SSL_CTX_new(TLSv1_2_client_method());
    if (m_ssl_ctx == nullptr) {
        IO_LOG(m_loop, ERROR, "Failed to init SSL context");
        return false;
    }

    result = SSL_CTX_set_cipher_list(m_ssl_ctx, "ALL:!SHA256:!SHA384:!aPSK:!ECDSA+SHA1:!ADH:!LOW:!EXP:!MD5");
    if (result == 0) {
        IO_LOG(m_loop, ERROR, "Failed to set siphers list");
        return false;
    }

    // TODO: most likely need to set also
    // SSL_CTX_set_verify
    // SSL_CTX_set_verify_depth

    m_ssl = SSL_new(m_ssl_ctx);
    if (m_ssl == nullptr) {
        IO_LOG(m_loop, ERROR, "Failed to create SSL");
        return false;
    }

    m_ssl_read_bio = BIO_new(BIO_s_mem());
    if (m_ssl_read_bio == nullptr) {
        IO_LOG(m_loop, ERROR, "Failed to create read BIO");
        return false;
    }

    m_ssl_write_bio = BIO_new(BIO_s_mem());
    if (m_ssl_write_bio == nullptr) {
        IO_LOG(m_loop, ERROR, "Failed to create write BIO");
        return false;
    }

    SSL_set_bio(m_ssl, m_ssl_read_bio, m_ssl_write_bio);

    IO_LOG(m_loop, DEBUG, "SSL inited");

    return true;
}
*/
TlsTcpClient::Impl::Impl(EventLoop& loop, TlsTcpClient& parent) :
    TlsTcpClientImplBase(loop, parent),
    m_loop(&loop),
    m_parent(&parent),
    m_tcp_client(new TcpClient(loop)) {
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

void TlsTcpClient::Impl::do_handshake() {
    auto handshake_result = SSL_do_handshake(m_ssl);

    int write_pending = BIO_pending(m_ssl_write_bio);
    int read_pending = BIO_pending(m_ssl_read_bio);
    IO_LOG(m_loop, TRACE, "write_pending:", write_pending);
    IO_LOG(m_loop, TRACE, "read_pending:", read_pending);

    if (handshake_result < 0) {
        auto error = SSL_get_error(m_ssl, handshake_result);

        if (error == SSL_ERROR_WANT_READ) {
            IO_LOG(m_loop, TRACE, "SSL_ERROR_WANT_READ");

            const std::size_t BUF_SIZE = 4096;
            std::shared_ptr<char> buf(new char[BUF_SIZE], [](const char* p) { delete[] p; });
            const auto size = BIO_read(m_ssl_write_bio, buf.get(), BUF_SIZE);

            IO_LOG(m_loop, TRACE, "Getting data from SSL and sending to server, size:", size);
            m_tcp_client->send_data(buf, size);
        } else if (error == SSL_ERROR_WANT_WRITE) {
            IO_LOG(m_loop, TRACE, "SSL_ERROR_WANT_WRITE");
        } else {
            char buf[4096];
            ERR_error_string_n(SSL_get_error(m_ssl, error), buf, 4096);
            IO_LOG(m_loop, ERROR, buf);
            // TODO: error handling here
        }
    } else if (handshake_result == 1) {
        IO_LOG(m_loop, TRACE, "Connected!!!!");
        m_ssl_handshake_complete = true;

        if (m_connect_callback) {
            m_connect_callback(*this->m_parent, Error(0));
        }

        if (read_pending) {
            IO_LOG(m_loop, TRACE, "read_pending:", read_pending);
            read_from_ssl();
        }

    } else {
        IO_LOG(m_loop, ERROR, "The TLS/SSL handshake was not successful but was shut down controlled and by the specifications of the TLS/SSL protocol.");
    }
}

/*
void TlsTcpClient::Impl::read_from_ssl() {
    const std::size_t SIZE = 16*1024; // https://www.openssl.org/docs/man1.0.2/man3/SSL_read.html
    std::unique_ptr<char[]> decrypted_buf(new char[SIZE]);

    int decrypted_size = SSL_read(m_ssl, decrypted_buf.get(), SIZE);
    IO_LOG(m_loop, TRACE, "Decrypted message size:", decrypted_size);
    while (decrypted_size > 0) {
        //IO_LOG(m_loop, TRACE, "Decrypted message size:", decrypted_size, "original_size:", size);
        m_receive_callback(*this->m_parent, decrypted_buf.get(), decrypted_size);

        decrypted_size = SSL_read(m_ssl, decrypted_buf.get(), SIZE);
    }

    if (decrypted_size < 0) {
        int code = SSL_get_error(m_ssl, decrypted_size);
        if (code != SSL_ERROR_WANT_READ) {
            IO_LOG(m_loop, ERROR, "Failed to write buf of size", code);
            // TODO: handle error
            return;
        }
    }
}
*/

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

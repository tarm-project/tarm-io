#include "DtlsServer.h"

#include "Common.h"
#include "detail/DtlsContext.h"
#include "UdpServer.h"
#include "UdpPeer.h"

#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/bn.h>

#include <iostream>
#include <memory>
#include <cstdio>

namespace io {

class DtlsServer::Impl {
public:
    Impl(EventLoop& loop, const Path& certificate_path, const Path& private_key_path, DtlsVersionRange version_range, DtlsServer& parent);
    ~Impl();

    Error listen(const std::string& ip_addr_str,
                 std::uint16_t port,
                 NewConnectionCallback new_connection_callback,
                 DataReceivedCallback data_receive_callback);

    void shutdown();
    void close();

    std::size_t connected_clients_count() const;

    bool certificate_and_key_match();

protected: // callbacks
    void on_new_peer(UdpServer& server, UdpPeer& udp_client, const io::Error& error);
    void on_data_receive(UdpServer& server, UdpPeer& udp_client, const DataChunk& data, const Error& error);
    void on_timeout(UdpServer& server, UdpPeer& udp_client);

private:
    using X509Ptr = std::unique_ptr<::X509, decltype(&::X509_free)>;
    using EvpPkeyPtr = std::unique_ptr<::EVP_PKEY, decltype(&::EVP_PKEY_free)>;

    DtlsServer* m_parent;
    EventLoop* m_loop;
    UdpServer* m_udp_server;

    Path m_certificate_path;
    Path m_private_key_path;

    X509Ptr m_certificate;
    EvpPkeyPtr m_private_key;
    DtlsVersionRange m_version_range;

    NewConnectionCallback m_new_connection_callback = nullptr;
    DataReceivedCallback m_data_receive_callback = nullptr;
};

DtlsServer::Impl::Impl(EventLoop& loop, const Path& certificate_path, const Path& private_key_path, DtlsVersionRange version_range, DtlsServer& parent) :
    m_parent(&parent),
    m_loop(&loop),
    m_udp_server(new UdpServer(loop)),
    m_certificate_path(certificate_path),
    m_private_key_path(private_key_path),
    m_certificate(nullptr, ::X509_free),
    m_private_key(nullptr, ::EVP_PKEY_free),
    m_version_range(version_range) {
}

DtlsServer::Impl::~Impl() {
    m_udp_server->schedule_removal();
}

void DtlsServer::Impl::on_new_peer(UdpServer& server, UdpPeer& udp_client, const io::Error& error) {
    if (error) {
        //m_new_connection_callback(.......);
        // TODO: error handling here
        return;
    }

    detail::DtlsContext context {
        m_certificate.get(),
        m_private_key.get(),
        m_version_range
    };

    DtlsConnectedClient* dtls_client =
        new DtlsConnectedClient(*m_loop, *m_parent, m_new_connection_callback, udp_client, &context);
    udp_client.set_on_schedule_removal(
        [=](const Removable&) {
            delete dtls_client;
        }
    );

    Error init_error = dtls_client->init_ssl();
    if (!init_error) {
        dtls_client->set_data_receive_callback(m_data_receive_callback);
    } else {
        // TODO: error
    }
}

void DtlsServer::Impl::on_data_receive(UdpServer& server, UdpPeer& udp_client, const DataChunk& data, const Error& error) {
    auto& dtls_client = *reinterpret_cast<DtlsConnectedClient*>(udp_client.user_data());
    dtls_client.on_data_receive(data.buf.get(), data.size);
}

void DtlsServer::Impl::on_timeout(UdpServer& server, UdpPeer& udp_client) {
    IO_LOG(this->m_loop, TRACE, "Removing DTLS client");

    udp_client.set_on_schedule_removal(nullptr);

    auto& dtls_client = *reinterpret_cast<DtlsConnectedClient*>(udp_client.user_data());
    delete &dtls_client;
}

Error DtlsServer::Impl::listen(const std::string& ip_addr_str,
                               std::uint16_t port,
                               NewConnectionCallback new_connection_callback,
                               DataReceivedCallback data_receive_callback) {
    m_new_connection_callback = new_connection_callback;
    m_data_receive_callback = data_receive_callback;

    using FilePtr = std::unique_ptr<FILE, decltype(&std::fclose)>;

    FilePtr certificate_file(std::fopen(m_certificate_path.string().c_str(), "r"), &std::fclose);
    if (certificate_file == nullptr) {
        return Error(StatusCode::TLS_CERTIFICATE_FILE_NOT_EXIST);
    }

    m_certificate.reset(PEM_read_X509(certificate_file.get(), nullptr, nullptr, nullptr));
    if (m_certificate == nullptr) {
        return Error(StatusCode::TLS_CERTIFICATE_INVALID);
    }

    FilePtr private_key_file(std::fopen(m_private_key_path.string().c_str(), "r"), &std::fclose);
    if (private_key_file == nullptr) {
        return Error(StatusCode::TLS_PRIVATE_KEY_FILE_NOT_EXIST);
    }

    m_private_key.reset(PEM_read_PrivateKey(private_key_file.get(), nullptr, nullptr, nullptr));
    if (m_private_key == nullptr) {
        return Error(StatusCode::TLS_PRIVATE_KEY_INVALID);
    }

    // TODO: check unsigned long err = ERR_get_error(); ?????

    if (!certificate_and_key_match()) {
        return Error(StatusCode::TLS_PRIVATE_KEY_AND_CERTIFICATE_NOT_MATCH);
    }

    using namespace std::placeholders;
    return m_udp_server->start_receive(ip_addr_str,
                                       port,
                                       std::bind(&DtlsServer::Impl::on_new_peer, this, _1, _2, _3),
                                       std::bind(&DtlsServer::Impl::on_data_receive, this, _1, _2, _3, _4),
                                       1000000, // TODO: hardcoded value
                                       std::bind(&DtlsServer::Impl::on_timeout, this, _1, _2));
}

void DtlsServer::Impl::shutdown() {
    // TODO: fixme
    //return m_udp_server->shutdown();
}

void DtlsServer::Impl::close() {
    // TODO: fixme
    //return m_udp_server->close();
}

std::size_t DtlsServer::Impl::connected_clients_count() const {
    //return m_udp_server->connected_clients_count();
    // TODO: fixme
    return 0;
}

namespace {

//int ssl_key_type(::EVP_PKEY* pkey) {
//    assert(pkey);
//    return pkey ? EVP_PKEY_type(pkey->type) : NID_undef;
//}

} // namespace

bool DtlsServer::Impl::certificate_and_key_match() {
    assert(m_certificate);
    assert(m_private_key);

    return X509_verify(m_certificate.get(), m_private_key.get()) != 0;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

IO_DEFINE_DEFAULT_MOVE(DtlsServer);

DtlsServer::DtlsServer(EventLoop& loop, const Path& certificate_path, const Path& private_key_path, DtlsVersionRange version_range) :
    Removable(loop),
    m_impl(new Impl(loop, certificate_path, private_key_path, version_range, *this)) {
}

DtlsServer::~DtlsServer() {
}

Error DtlsServer::listen(const std::string& ip_addr_str,
                           std::uint16_t port,
                           DataReceivedCallback data_receive_callback) {
    return m_impl->listen(ip_addr_str, port, nullptr, data_receive_callback);
}

Error DtlsServer::listen(const std::string& ip_addr_str,
                           std::uint16_t port,
                           NewConnectionCallback new_connection_callback,
                           DataReceivedCallback data_receive_callback) {
    return m_impl->listen(ip_addr_str, port, new_connection_callback, data_receive_callback);
}

void DtlsServer::shutdown() {
    return m_impl->shutdown();
}

void DtlsServer::close() {
    return m_impl->close();
}

std::size_t DtlsServer::connected_clients_count() const {
    return m_impl->connected_clients_count();
}

} // namespace io


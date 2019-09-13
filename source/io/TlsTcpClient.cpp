#include "TlsTcpClient.h"

#include <openssl/ssl.h>

namespace io {

class TlsTcpClient::Impl {
public:
    Impl(EventLoop& loop, TlsTcpClient& parent);

private:
    TlsTcpClient* m_parent;
    EventLoop* m_loop;
};

TlsTcpClient::Impl::Impl(EventLoop& loop, TlsTcpClient& parent) :
    m_loop(&loop),
    m_parent(&parent) {
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TlsTcpClient::TlsTcpClient(EventLoop& loop) :
    Disposable(loop),
    m_impl(new Impl(loop, *this)) {
    SSL_load_error_strings();
}

TlsTcpClient::~TlsTcpClient() {

}

} // namespace io
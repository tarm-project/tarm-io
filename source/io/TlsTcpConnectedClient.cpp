#include "TlsTcpConnectedClient.h"

namespace io {

class TlsTcpConnectedClient::Impl {
public:
    Impl(EventLoop& loop, TlsTcpConnectedClient& parent);
    ~Impl();

private:
    TlsTcpConnectedClient* m_parent;
    EventLoop* m_loop;
};

TlsTcpConnectedClient::Impl::Impl(EventLoop& loop, TlsTcpConnectedClient& parent) :
    m_parent(&parent),
    m_loop(&loop) {
}

TlsTcpConnectedClient::Impl::~Impl() {
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TlsTcpConnectedClient::TlsTcpConnectedClient(EventLoop& loop) :
    Disposable(loop),
    m_impl(new Impl(loop, *this)) {
}


TlsTcpConnectedClient::~TlsTcpConnectedClient() {
}

} // namespace io


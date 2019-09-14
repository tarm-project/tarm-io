#include "TlsTcpServer.h"

namespace io {

class TlsTcpServer::Impl {
public:
    Impl(EventLoop& loop, TlsTcpServer& parent);
    ~Impl();

private:
    TlsTcpServer* m_parent;
    EventLoop* m_loop;
};

TlsTcpServer::Impl::Impl(EventLoop& loop, TlsTcpServer& parent) :
    m_parent(&parent),
    m_loop(&loop) {
}

TlsTcpServer::Impl::~Impl() {
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

TlsTcpServer::TlsTcpServer(EventLoop& loop) :
    Disposable(loop),
    m_impl(new Impl(loop, *this)) {
}


TlsTcpServer::~TlsTcpServer() {
}

} // namespace io

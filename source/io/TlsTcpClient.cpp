#include "TlsTcpClient.h"

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
}

TlsTcpClient::~TlsTcpClient() {

}

} // namespace io
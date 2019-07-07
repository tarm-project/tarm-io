#include "UdpServer.h"

namespace io {

class UdpServer::Impl {
public:
    Impl(EventLoop& loop);

private:
    EventLoop* m_loop = nullptr;
    uv_loop_t* m_uv_loop = nullptr;
};

UdpServer::Impl::Impl(EventLoop& loop) :
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())){
}

///////////////////////////////////////////  static  ////////////////////////////////////////////

/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpServer::UdpServer(EventLoop& loop) :
    Disposable(loop),
    m_impl(new UdpServer::Impl(loop)) {
}

UdpServer::~UdpServer() {

}

} // namespace io

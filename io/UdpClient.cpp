#include "UdpClient.h"

namespace io {

class UdpClient::Impl {
public:
    Impl(EventLoop& loop);

private:
    EventLoop* m_loop = nullptr;
    uv_loop_t* m_uv_loop = nullptr;
};

UdpClient::Impl::Impl(EventLoop& loop) :
    m_loop(&loop),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())){
}

///////////////////////////////////////////  static  ////////////////////////////////////////////

/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpClient::UdpClient(EventLoop& loop) :
    Disposable(loop),
    m_impl(new UdpClient::Impl(loop)) {
}

UdpClient::~UdpClient() {

}

} // namespace io

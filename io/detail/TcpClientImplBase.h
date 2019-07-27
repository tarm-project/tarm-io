#pragma once

namespace io {
namespace detail {

template<typename BaseType>
class TcpClientImplBase {
public:
    TcpClientImplBase(EventLoop& loop) :
        m_loop(&loop),
        m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())) {
    }

protected:
    EventLoop* m_loop;
    uv_loop_t* m_uv_loop;

private:

};

} // namespace detail
} // namespace io

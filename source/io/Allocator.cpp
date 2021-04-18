#include "Allocator.h"
//*
namespace tarm {
namespace io {

TARM_IO_DLL_PUBLIC thread_local Error Allocator::m_last_allocation_error{StatusCode::OK};

} // namespace io
} // namespace tarm
//*/
/*
#include "Timer.h"
#include "fs/Dir.h"
#include "fs/File.h"

#ifdef TARM_IO_HAS_OPENSSL
    #include "net/TlsClient.h"
    #include "net/TlsServer.h"
#endif

namespace tarm {
namespace io {

class Allocator::Impl {
public:
    Impl(EventLoop& loop);
    ~Impl();

    TARM_IO_FORBID_COPY(Impl)
    TARM_IO_FORBID_MOVE(Impl);

    template<typename T, typename... ARGS>
    T* allocate(ARGS&&... args) {
        Error error;
        auto ptr = new(std::nothrow) T(*m_loop, error, std::forward<ARGS>(args)...);
        m_last_allocation_error = error;
        if (error) {
            return nullptr;
        }

        if (ptr == nullptr) {
            m_last_allocation_error = StatusCode::OUT_OF_MEMORY;
            return nullptr;
        }

        return ptr;
    }

    Error last_allocation_error() const;

private:
    EventLoop* m_loop;
    static thread_local Error m_last_allocation_error;
};

thread_local Error Allocator::Impl::m_last_allocation_error{StatusCode::OK};

Allocator::Impl::Impl(EventLoop& loop) :
    m_loop(&loop) {
}

Allocator::Impl::~Impl() {
}

Error Allocator::Impl::last_allocation_error() const {
    return m_last_allocation_error;
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

Allocator::Allocator(EventLoop& loop) :
    m_impl(new Allocator::Impl(loop)) {

}

Allocator::~Allocator() = default;

template<typename T>
T* Allocator::allocate() {
    return m_impl->allocate<T>();
}

template<typename T, typename... ARGS>
T* Allocator::allocate(ARGS&&... args) {
    return m_impl->allocate<T>(std::forward<ARGS>(args)...);
}

Error Allocator::last_allocation_error() const {
    return m_impl->last_allocation_error();
}

template TARM_IO_DLL_PUBLIC Timer* Allocator::allocate<Timer>();
template TARM_IO_DLL_PUBLIC fs::Dir* Allocator::allocate<fs::Dir>();
template TARM_IO_DLL_PUBLIC fs::File* Allocator::allocate<fs::File>();

#ifdef TARM_IO_HAS_OPENSSL
    template TARM_IO_DLL_PUBLIC net::TlsClient* Allocator::allocate<net::TlsClient>();
    template TARM_IO_DLL_PUBLIC net::TlsClient* Allocator::allocate<net::TlsClient, net::TlsVersionRange>(net::TlsVersionRange&&);

    template TARM_IO_DLL_PUBLIC net::TlsServer* Allocator::allocate<net::TlsServer, fs::Path, fs::Path>(fs::Path&&, fs::Path&&);
    template TARM_IO_DLL_PUBLIC net::TlsServer* Allocator::allocate<net::TlsServer, fs::Path, fs::Path, net::TlsVersionRange>(fs::Path&&, fs::Path&&, net::TlsVersionRange&&);
#endif

} // namespace io
} // namespace tarm
//*/

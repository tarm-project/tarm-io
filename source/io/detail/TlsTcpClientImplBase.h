#pragma once

#include "io/EventLoop.h"

#include <openssl/ssl.h>

//#include <memory>
//#include <assert.h>

namespace io {
namespace detail {

template<typename ParentType, typename ImplType>
class TlsTcpClientImplBase {
public:
    TlsTcpClientImplBase(EventLoop& loop, ParentType& parent);
    ~TlsTcpClientImplBase();

protected:
    bool m_ssl_handshake_complete = false;

    SSL_CTX* m_ssl_ctx = nullptr;
    BIO* m_ssl_read_bio = nullptr;
    BIO* m_ssl_write_bio = nullptr;
    SSL* m_ssl = nullptr;
};

///////////////////////////////////////// implementation ///////////////////////////////////////////

template<typename ParentType, typename ImplType>
TlsTcpClientImplBase<ParentType, ImplType>::TlsTcpClientImplBase(EventLoop& loop, ParentType& parent) {

}

template<typename ParentType, typename ImplType>
TlsTcpClientImplBase<ParentType, ImplType>::~TlsTcpClientImplBase() {

}

} // namespace detail
} // namespace io
#include "Status.h"

#include "Common.h"

namespace io {

Status::Status(int libuv_code) :
    m_libuv_code(libuv_code),
    m_status_code(convert_from_uv(libuv_code)) {
}

StatusCode Status::status_code() const {
    return m_status_code;
}

std::string Status::as_string() const {
    if (m_status_code == StatusCode::OK) {
        return "OK";
    } else if (m_status_code == StatusCode::UNDEFINED) {
        return "Unknow status code from libuv: " + std::to_string(m_libuv_code);
    }

    return uv_strerror(m_libuv_code);
}

bool Status::ok() const {
    return m_status_code == StatusCode::OK;
}

bool Status::fail() const {
    return m_status_code != StatusCode::OK;
}

} // namespace io

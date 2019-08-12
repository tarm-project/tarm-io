#include "Status.h"

#include "Common.h"

namespace io {

Status::Status(int libuv_code) {
    if (libuv_code < 0) {
        m_libuv_code = libuv_code;
        m_status_code = convert_from_uv(libuv_code);
    }
}

Status::Status(StatusCode status_code) :
   m_status_code(status_code) {
}

StatusCode Status::code() const {
    return m_status_code;
}

std::string Status::as_string() const {
    if (m_status_code == StatusCode::OK) {
        return "OK";
    } else if (m_status_code == StatusCode::FILE_NOT_OPEN) {
            return "File is not opened";
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

///////////////////////////////// free functions /////////////////////////////////

bool operator==(const Status& s1, const Status& s2) {
    return s1.code() == s2.code();
}

bool operator!=(const Status& s1, const Status& s2) {
    return !operator==(s1, s2);
}

} // namespace io

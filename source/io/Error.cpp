#include "Error.h"

#include "Common.h"

namespace io {

Error::Error(std::int64_t libuv_code) {
    if (libuv_code < 0) {
        m_libuv_code = libuv_code;
        m_status_code = convert_from_uv(libuv_code);
    }
}

Error::Error(StatusCode status_code) :
   m_status_code(status_code) {
}

StatusCode Error::code() const {
    return m_status_code;
}

std::string Error::string() const {
    if (m_status_code == StatusCode::OK) {
        return "No error. Status OK";
    } else if (m_status_code == StatusCode::FILE_NOT_OPEN) {
        return "File is not opened";
    } else if (m_status_code == StatusCode::TLS_CERTIFICATE_ERROR_FILE_NOT_EXIST) {
        return "Certificate error. File does not exist";
    } else if (m_status_code == StatusCode::TLS_PRIVATE_KEY_ERROR_FILE_NOT_EXIST) {
        return "Private key error. File does not exist";
    } else if (m_status_code == StatusCode::UNDEFINED) {
        return "Unknow status code from libuv: " + std::to_string(m_libuv_code);
    }

    return uv_strerror(static_cast<int>(m_libuv_code));
}

Error::operator bool() const {
    return m_status_code != StatusCode::OK;
}

///////////////////////////////// free functions /////////////////////////////////

bool operator==(const Error& s1, const Error& s2) {
    return s1.code() == s2.code();
}

bool operator!=(const Error& s1, const Error& s2) {
    return !operator==(s1, s2);
}

} // namespace io

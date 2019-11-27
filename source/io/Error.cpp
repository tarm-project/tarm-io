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

Error::Error(StatusCode status_code, const std::string& custom_error_message) :
    m_status_code(status_code),
    m_custom_error_message(custom_error_message) {
}

StatusCode Error::code() const {
    return m_status_code;
}

std::string Error::string() const {
    switch (m_status_code) {
        case StatusCode::OK:
            return "No error. Status OK";
        case StatusCode::FILE_NOT_OPEN:
            return "File is not opened";
        case StatusCode::TLS_CERTIFICATE_FILE_NOT_EXIST:
            return "Certificate error. File does not exist";
        case StatusCode::TLS_PRIVATE_KEY_FILE_NOT_EXIST:
            return "Private key error. File does not exist";
        case StatusCode::TLS_CERTIFICATE_INVALID:
            return "Certificate error. Certificate is invalid or corrupted";
        case StatusCode::TLS_PRIVATE_KEY_INVALID:
            return "Private key error. Private key is invalid or corrupted";
        case StatusCode::TLS_PRIVATE_KEY_AND_CERTIFICATE_NOT_MATCH:
            return "Private key and certificate do not match";

        case StatusCode::OPENSSL_ERROR:
            return "Openssl error: " + m_custom_error_message;

        case StatusCode::UNDEFINED:
            return "Unknow status code from libuv: " + std::to_string(m_libuv_code);

        default:
            return uv_strerror(static_cast<int>(m_libuv_code));
    }
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

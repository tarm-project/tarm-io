#pragma once

#include "Export.h"
#include "StatusCode.h"

#include <string>

namespace io {

class IO_DLL_PUBLIC Error {
public:
    Error(std::int64_t libuv_code);
    Error(StatusCode status_code);
    Error(StatusCode status_code, const std::string& custom_error_message);

    StatusCode code() const;
    std::string string() const;

    operator bool() const;

private:
    std::int64_t m_libuv_code = 0;
    StatusCode m_status_code = StatusCode::OK;
    std::string m_custom_error_message;
};

IO_DLL_PUBLIC
bool operator==(const Error& s1, const Error& s2);

IO_DLL_PUBLIC
bool operator!=(const Error& s1, const Error& s2);

} // namespace io

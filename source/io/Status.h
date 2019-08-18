#pragma once

#include "Export.h"
#include "StatusCode.h"

#include <string>

namespace io {

class IO_DLL_PUBLIC Status {
public:
    Status(ssize_t libuv_code);
    Status(StatusCode status_code);

    StatusCode code() const;
    std::string as_string() const;

    bool ok() const;
    bool fail() const;

private:
    ssize_t m_libuv_code = 0;
    StatusCode m_status_code = StatusCode::OK;
};

IO_DLL_PUBLIC
bool operator==(const Status& s1, const Status& s2);

IO_DLL_PUBLIC
bool operator!=(const Status& s1, const Status& s2);

} // namespace io

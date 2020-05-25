/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "StatusCode.h"

#include "detail/Common.h"

#include <assert.h>
#include <ostream>

namespace tarm {
namespace io {

StatusCode convert_from_uv(std::int64_t libuv_code) {
    switch (libuv_code) {
        case 0:
            return StatusCode::OK;
        case UV_E2BIG:
            return StatusCode::ARGUMENT_LIST_TOO_LONG;
        case UV_EACCES:
            return StatusCode::PERMISSION_DENIED;
        case UV_EADDRINUSE:
            return StatusCode::ADDRESS_ALREADY_IN_USE;
        case UV_EADDRNOTAVAIL:
            return StatusCode::ADDRESS_NOT_AVAILABLE;
        case UV_EAFNOSUPPORT:
            return StatusCode::ADDRESS_FAMILY_NOT_SUPPORTED;
        case UV_EAGAIN:
            return StatusCode::RESOURCE_TEMPORARILY_UNAVAILABLE;
        case UV_EAI_ADDRFAMILY:
            return StatusCode::ADDRESS_FAMILY_NOT_SUPPORTED;
        case UV_EAI_AGAIN:
            return StatusCode::TEMPORARY_FAILURE;
        case UV_EAI_BADFLAGS:
            return StatusCode::BAD_AI_FLAGS_VALUE;
        case UV_EAI_BADHINTS:
            return StatusCode::INVALID_VALUE_FOR_HINTS;
        case UV_EAI_CANCELED:
            return StatusCode::REQUEST_CANCELED;
        case UV_EAI_FAIL:
            return StatusCode::PERMANENT_FAILURE;
        case UV_EAI_FAMILY:
            return StatusCode::AI_FAMILY_NOT_SUPPORTED;
        case UV_EAI_MEMORY:
            return StatusCode::OUT_OF_MEMORY;
        case UV_EAI_NODATA:
            return StatusCode::NO_ADDRESS_AVAILABLE;
        case UV_EAI_NONAME:
            return StatusCode::UNKNOWN_NODE_OR_SERVICE;
        case UV_EAI_OVERFLOW:
            return StatusCode::ARGUMENT_BUFFER_OVERFLOW;
        case UV_EAI_PROTOCOL:
            return StatusCode::RESOLVED_PROTOCOL_IS_UNKNOWN;
        case UV_EAI_SERVICE:
            return StatusCode::SERVICE_NOT_AVAILABLE_FOR_SOCKET_TYPE;
        case UV_EAI_SOCKTYPE:
            return StatusCode::SOCKET_TYPE_NOT_SUPPORTED;
        case UV_EALREADY:
            return StatusCode::CONNECTION_ALREADY_IN_PROGRESS;
        case UV_EBADF:
            return StatusCode::BAD_FILE_DESCRIPTOR;
        case UV_EBUSY:
            return StatusCode::RESOURCE_BUSY_OR_LOCKED;
        case UV_ECANCELED:
            return StatusCode::OPERATION_CANCELED;
        case UV_ECHARSET:
            return StatusCode::INVALID_UNICODE_CHARACTER;
        case UV_ECONNABORTED:
            return StatusCode::SOFTWARE_CAUSED_CONNECTION_ABORT;
        case UV_ECONNREFUSED:
            return StatusCode::CONNECTION_REFUSED;
        case UV_ECONNRESET:
            return StatusCode::CONNECTION_RESET_BY_PEER;
        case UV_EDESTADDRREQ:
            return StatusCode::DESTINATION_ADDRESS_REQUIRED;
        case UV_EEXIST:
            return StatusCode::FILE_OR_DIR_ALREADY_EXISTS;
        case UV_EFAULT:
            return StatusCode::BAD_ADDRESS_IN_SYSTEM_CALL_ARGUMENT;
        case UV_EFBIG:
            return StatusCode::FILE_TOO_LARGE;
        case UV_EHOSTUNREACH:
            return StatusCode::HOST_IS_UNREACHABLE;
        case UV_EINTR:
            return StatusCode::INTERRUPTED_SYSTEM_CALL;
        case UV_EINVAL:
            return StatusCode::INVALID_ARGUMENT;
        case UV_EIO:
            return StatusCode::IO_ERROR;
        case UV_EISCONN:
            return StatusCode::SOCKET_IS_ALREADY_CONNECTED;
        case UV_EISDIR:
            return StatusCode::ILLEGAL_OPERATION_ON_A_DIRECTORY;
        case UV_ELOOP:
            return StatusCode::TOO_MANY_SYMBOLIC_LINKS_ENCOUNTERED;
        case UV_EMFILE:
            return StatusCode::TOO_MANY_OPEN_FILES;
        case UV_EMSGSIZE:
            return StatusCode::MESSAGE_TOO_LONG;
        case UV_ENAMETOOLONG:
            return StatusCode::NAME_TOO_LONG;
        case UV_ENETDOWN:
            return StatusCode::NETWORK_IS_DOWN;
        case UV_ENETUNREACH:
            return StatusCode::NETWORK_IS_UNREACHABLE;
        case UV_ENFILE:
            return StatusCode::FILE_TABLE_OVERFLOW;
        case UV_ENOBUFS:
            return StatusCode::NO_BUFFER_SPACE_AVAILABLE;
        case UV_ENODEV:
            return StatusCode::NO_SUCH_DEVICE;
        case UV_ENOENT:
            return StatusCode::NO_SUCH_FILE_OR_DIRECTORY;
        case UV_ENOMEM:
            return StatusCode::NOT_ENOUGH_MEMORY;
        case UV_ENONET:
            return StatusCode::MACHINE_IS_NOT_ON_THE_NETWORK;
        case UV_ENOPROTOOPT:
            return StatusCode::PROTOCOL_NOT_AVAILABLE;
        case UV_ENOSPC:
            return StatusCode::NO_SPACE_LEFT_ON_DEVICE;
        case UV_ENOSYS:
            return StatusCode::FUNCTION_NOT_IMPLEMENTED;
        case UV_ENOTCONN:
            return StatusCode::SOCKET_IS_NOT_CONNECTED;
        case UV_ENOTDIR:
            return StatusCode::NOT_A_DIRECTORY;
        case UV_ENOTEMPTY:
            return StatusCode::DIRECTORY_NOT_EMPTY;
        case UV_ENOTSOCK:
            return StatusCode::SOCKET_OPERATION_ON_NON_SOCKET;
        case UV_ENOTSUP:
            return StatusCode::OPERATION_NOT_SUPPORTED_ON_SOCKET;
        case UV_EPERM:
            return StatusCode::OPERATION_NOT_PERMITTED;
        case UV_EPIPE:
            return StatusCode::BROKEN_PIPE;
        case UV_EPROTO:
            return StatusCode::PROTOCOL_ERROR;
        case UV_EPROTONOSUPPORT:
            return StatusCode::PROTOCOL_NOT_SUPPORTED;
        case UV_EPROTOTYPE:
            return StatusCode::PROTOCOL_WRONG_TYPE_FOR_SOCKET;
        case UV_ERANGE:
            return StatusCode::RESULT_TOO_LARGE;
        case UV_EROFS:
            return StatusCode::READ_ONLY_FILE_SYSTEM;
        case UV_ESHUTDOWN:
            return StatusCode::CANNOT_SEND_AFTER_TRANSPORT_ENDPOINT_SHUTDOWN;
        case UV_ESPIPE:
            return StatusCode::INVALID_SEEK;
        case UV_ESRCH:
            return StatusCode::NO_SUCH_PROCESS;
        case UV_ETIMEDOUT:
            return StatusCode::CONNECTION_TIMED_OUT;
        case UV_ETXTBSY:
            return StatusCode::TEXT_FILE_IS_BUSY;
        case UV_EXDEV:
            return StatusCode::CROSS_DEVICE_LINK_NOT_PERMITTED;
        case UV_UNKNOWN:
            return StatusCode::UNKNOWN_ERROR;
        case UV_EOF:
            return StatusCode::END_OF_FILE;
        case UV_ENXIO:
            return StatusCode::NO_SUCH_DEVICE_OR_ADDRESS;
        case UV_EMLINK:
            return StatusCode::TOO_MANY_LINKS;
        case UV_EHOSTDOWN:
            return StatusCode::HOST_IS_DOWN;
        case UV_EREMOTEIO:
            return StatusCode::REMOTE_IO_ERROR;
        case UV_ENOTTY:
            return StatusCode::INAPPROPRIATE_IOCTL_FOR_DEVICE;
        case UV_EFTYPE:
            return StatusCode::INAPPROPRIATE_FILE_TYPE_OR_FORMAT;
    }

    return StatusCode::UNDEFINED;
}

std::ostream& operator<<(std::ostream& out, StatusCode code) {
    switch(code) {
        #define X(PARAM) case StatusCode::PARAM: out << #PARAM; break;
        IO_LIST_OF_STATUS_CODES
        #undef X
        default:
            assert(false);
    };

    return out;
}

} // namespace io
} // namespace tarm

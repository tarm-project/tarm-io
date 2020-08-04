/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "Error.h"

#include "detail/Common.h"

#include <assert.h>

namespace tarm {
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
        case StatusCode::UNDEFINED:
            return "Unknown error";
        case StatusCode::FILE_NOT_OPEN:
            return "File is not opened";
        case StatusCode::DIR_NOT_OPEN:
            return "Directory is not opened";
        case StatusCode::TRANSPORT_INIT_FAILURE:
            return "Failed to init underlying transport or protocol";
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
        case StatusCode::NOT_CONNECTED:
            return "Not connected";
        case StatusCode::OPENSSL_ERROR:
            return "Openssl error: " + m_custom_error_message;

        // UV codes
        case StatusCode::ARGUMENT_LIST_TOO_LONG:
            return "Argument list too long";
        case StatusCode::PERMISSION_DENIED:
            return "Permission denied";
        case StatusCode::ADDRESS_ALREADY_IN_USE:
            return "Address already in use";
        case StatusCode::ADDRESS_NOT_AVAILABLE:
            return "Address not available";
        case StatusCode::RESOURCE_TEMPORARILY_UNAVAILABLE:
            return "Resource temporarily unavailable";
        case StatusCode::ADDRESS_FAMILY_NOT_SUPPORTED:
            return "Address family not supported";
        case StatusCode::TEMPORARY_FAILURE:
            return "Temporary failure";
        case StatusCode::BAD_AI_FLAGS_VALUE:
            return "Bad ai_flags value";
        case StatusCode::INVALID_VALUE_FOR_HINTS:
            return "Invalid value for hints";
        case StatusCode::REQUEST_CANCELED:
            return "Request canceled";
        case StatusCode::PERMANENT_FAILURE:
            return "Permanent failure";
        case StatusCode::AI_FAMILY_NOT_SUPPORTED:
            return "Ai_family not supported";
        case StatusCode::OUT_OF_MEMORY:
            return "Out of memory";
        case StatusCode::NO_ADDRESS_AVAILABLE:
            return "No address";
        case StatusCode::UNKNOWN_NODE_OR_SERVICE:
            return "Unknown node or service";
        case StatusCode::ARGUMENT_BUFFER_OVERFLOW:
            return "Argument buffer overflow";
        case StatusCode::RESOLVED_PROTOCOL_IS_UNKNOWN:
            return "Resolved protocol is unknown";
        case StatusCode::SERVICE_NOT_AVAILABLE_FOR_SOCKET_TYPE:
            return "Service not available for socket type";
        case StatusCode::SOCKET_TYPE_NOT_SUPPORTED:
            return "Socket type not supported";
        case StatusCode::CONNECTION_ALREADY_IN_PROGRESS:
            return "Connection already in progress";
        case StatusCode::BAD_FILE_DESCRIPTOR:
            return "Bad file descriptor";
        case StatusCode::RESOURCE_BUSY_OR_LOCKED:
            return "Resource busy or locked";
        case StatusCode::OPERATION_CANCELED:
            return "Operation canceled";
        case StatusCode::INVALID_UNICODE_CHARACTER:
            return "Invalid Unicode character";
        case StatusCode::SOFTWARE_CAUSED_CONNECTION_ABORT:
            return "Software caused connection abort";
        case StatusCode::CONNECTION_REFUSED:
            return "Connection refused";
        case StatusCode::CONNECTION_RESET_BY_PEER:
            return "Connection reset by peer";
        case StatusCode::DESTINATION_ADDRESS_REQUIRED:
            return "Destination address required";
        case StatusCode::FILE_OR_DIR_ALREADY_EXISTS:
            return "File already exists";
        case StatusCode::BAD_ADDRESS_IN_SYSTEM_CALL_ARGUMENT:
            return "Bad address in system call argument";
        case StatusCode::FILE_TOO_LARGE:
            return "File too large";
        case StatusCode::HOST_IS_UNREACHABLE:
            return "Host is unreachable";
        case StatusCode::INTERRUPTED_SYSTEM_CALL:
            return "Interrupted system call";
        case StatusCode::INVALID_ARGUMENT:
            return "Invalid argument";
        case StatusCode::IO_ERROR:
            return "I/o error";
        case StatusCode::SOCKET_IS_ALREADY_CONNECTED:
            return "Socket is already connected";
        case StatusCode::ILLEGAL_OPERATION_ON_A_DIRECTORY:
            return "Illegal operation on a directory";
        case StatusCode::TOO_MANY_SYMBOLIC_LINKS_ENCOUNTERED:
            return "Too many symbolic links encountered";
        case StatusCode::TOO_MANY_OPEN_FILES:
            return "Too many open files";
        case StatusCode::MESSAGE_TOO_LONG:
            return "Message too long";
        case StatusCode::NAME_TOO_LONG:
            return "Name too long";
        case StatusCode::NETWORK_IS_DOWN:
            return "Network is down";
        case StatusCode::NETWORK_IS_UNREACHABLE:
            return "Network is unreachable";
        case StatusCode::FILE_TABLE_OVERFLOW:
            return "File table overflow";
        case StatusCode::NO_BUFFER_SPACE_AVAILABLE:
            return "No buffer space available";
        case StatusCode::NO_SUCH_DEVICE:
            return "No such device";
        case StatusCode::NO_SUCH_FILE_OR_DIRECTORY:
            return "No such file or directory";
        case StatusCode::NOT_ENOUGH_MEMORY:
            return "Not enough memory";
        case StatusCode::MACHINE_IS_NOT_ON_THE_NETWORK:
            return "Machine is not on the network";
        case StatusCode::PROTOCOL_NOT_AVAILABLE:
            return "Protocol not available";
        case StatusCode::NO_SPACE_LEFT_ON_DEVICE:
            return "No space left on device";
        case StatusCode::FUNCTION_NOT_IMPLEMENTED:
            return "Function not implemented";
        case StatusCode::SOCKET_IS_NOT_CONNECTED:
            return "Socket is not connected";
        case StatusCode::NOT_A_DIRECTORY:
            return "Not a directory";
        case StatusCode::DIRECTORY_NOT_EMPTY:
            return "Directory not empty";
        case StatusCode::SOCKET_OPERATION_ON_NON_SOCKET:
            return "Socket operation on non-socket";
        case StatusCode::OPERATION_NOT_SUPPORTED_ON_SOCKET:
            return "Operation not supported on socket";
        case StatusCode::OPERATION_NOT_PERMITTED:
            return "Operation not permitted";
        case StatusCode::OPERATION_ON_CLOSED_SOCKET:
            return "Operation on closed socket";
        case StatusCode::PROTOCOL_ERROR:
            return "Protocol error";
        case StatusCode::PROTOCOL_NOT_SUPPORTED:
            return "Protocol not supported";
        case StatusCode::PROTOCOL_WRONG_TYPE_FOR_SOCKET:
            return "Protocol wrong type for socket";
        case StatusCode::RESULT_TOO_LARGE:
            return "Result too large";
        case StatusCode::READ_ONLY_FILE_SYSTEM:
            return "Read-only file system";
        case StatusCode::CANNOT_SEND_AFTER_TRANSPORT_ENDPOINT_SHUTDOWN:
            return "Cannot send after transport endpoint shutdown";
        case StatusCode::INVALID_SEEK:
            return "Invalid seek";
        case StatusCode::NO_SUCH_PROCESS:
            return "No such process";
        case StatusCode::CONNECTION_TIMED_OUT:
            return "Connection timed out";
        case StatusCode::TEXT_FILE_IS_BUSY:
            return "Text file is busy";
        case StatusCode::CROSS_DEVICE_LINK_NOT_PERMITTED:
            return "Cross-device link not permitted";
        case StatusCode::UNKNOWN_ERROR:
            return "Unknown error";
        case StatusCode::END_OF_FILE:
            return "End of file";
        case StatusCode::NO_SUCH_DEVICE_OR_ADDRESS:
            return "No such device or address";
        case StatusCode::TOO_MANY_LINKS:
            return "Too many links";
        case StatusCode::HOST_IS_DOWN:
            return "Host is down";
        case StatusCode::REMOTE_IO_ERROR:
            return "Remote I/O error";
        case StatusCode::INAPPROPRIATE_IOCTL_FOR_DEVICE:
            return "Inappropriate ioctl for device";
        case StatusCode::INAPPROPRIATE_FILE_TYPE_OR_FORMAT:
            return "Inappropriate file type or format";

        default:
            assert(false && "Error code without string value");
            return "";
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

std::ostream& operator<<(std::ostream& os, const Error& err) {
    os << err.string();
    return os;
}

} // namespace io
} // namespace tarm

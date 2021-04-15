/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "Export.h"

#include <cstdint>
#include <iosfwd>

namespace tarm {
namespace io {

// Note: do not forget to revise this list on bundled libuv version update

// X-Macro idiom, for details see
// https://en.wikibooks.org/wiki/C_Programming/Preprocessor_directives_and_macros#X-Macros
#define TARM_IO_LIST_OF_STATUS_CODES \
    /*custom codes*/ \
    X(OK) \
    X(UNDEFINED) \
    X(FILE_NOT_OPEN) \
    X(DIR_NOT_OPEN) \
    X(TRANSPORT_INIT_FAILURE) \
    X(TLS_CERTIFICATE_FILE_NOT_EXIST) \
    X(TLS_PRIVATE_KEY_FILE_NOT_EXIST) \
    X(TLS_CERTIFICATE_INVALID) \
    X(TLS_PRIVATE_KEY_INVALID) \
    X(TLS_PRIVATE_KEY_AND_CERTIFICATE_NOT_MATCH) \
    X(NOT_CONNECTED) \
    X(OPERATION_ALREADY_IN_PROGRESS) \
    X(PEER_NOT_FOUND) \
    /* compound error which usually includes custom message string */ \
    X(OPENSSL_ERROR) \
    /*libuv codes*/ \
    X(ARGUMENT_LIST_TOO_LONG) \
    X(PERMISSION_DENIED) \
    X(ADDRESS_ALREADY_IN_USE) \
    X(ADDRESS_NOT_AVAILABLE) \
    X(ADDRESS_FAMILY_NOT_SUPPORTED) \
    X(RESOURCE_TEMPORARILY_UNAVAILABLE) \
    X(TEMPORARY_FAILURE) \
    X(BAD_AI_FLAGS_VALUE) \
    X(INVALID_VALUE_FOR_HINTS) \
    X(REQUEST_CANCELED) \
    X(PERMANENT_FAILURE) \
    X(AI_FAMILY_NOT_SUPPORTED) \
    X(OUT_OF_MEMORY) \
    X(NO_ADDRESS_AVAILABLE) \
    X(UNKNOWN_NODE_OR_SERVICE) \
    X(ARGUMENT_BUFFER_OVERFLOW) \
    X(RESOLVED_PROTOCOL_IS_UNKNOWN) \
    X(SERVICE_NOT_AVAILABLE_FOR_SOCKET_TYPE) \
    X(SOCKET_TYPE_NOT_SUPPORTED) \
    X(CONNECTION_ALREADY_IN_PROGRESS) \
    X(BAD_FILE_DESCRIPTOR) \
    X(RESOURCE_BUSY_OR_LOCKED) \
    X(OPERATION_CANCELED) \
    X(INVALID_UNICODE_CHARACTER) \
    X(SOFTWARE_CAUSED_CONNECTION_ABORT) \
    X(CONNECTION_REFUSED) \
    X(CONNECTION_RESET_BY_PEER) \
    X(DESTINATION_ADDRESS_REQUIRED) \
    X(FILE_OR_DIR_ALREADY_EXISTS) \
    X(BAD_ADDRESS_IN_SYSTEM_CALL_ARGUMENT) \
    X(FILE_TOO_LARGE) \
    X(HOST_IS_UNREACHABLE) \
    X(INTERRUPTED_SYSTEM_CALL) \
    X(INVALID_ARGUMENT) \
    X(IO_ERROR) \
    X(SOCKET_IS_ALREADY_CONNECTED) \
    X(ILLEGAL_OPERATION_ON_A_DIRECTORY) \
    X(TOO_MANY_SYMBOLIC_LINKS_ENCOUNTERED) \
    X(TOO_MANY_OPEN_FILES) \
    X(MESSAGE_TOO_LONG) \
    X(NAME_TOO_LONG) \
    X(NETWORK_IS_DOWN) \
    X(NETWORK_IS_UNREACHABLE) \
    X(FILE_TABLE_OVERFLOW) \
    X(NO_BUFFER_SPACE_AVAILABLE) \
    X(NO_SUCH_DEVICE) \
    X(NO_SUCH_FILE_OR_DIRECTORY) \
    X(NOT_ENOUGH_MEMORY) \
    X(MACHINE_IS_NOT_ON_THE_NETWORK) \
    X(PROTOCOL_NOT_AVAILABLE) \
    X(NO_SPACE_LEFT_ON_DEVICE) \
    X(FUNCTION_NOT_IMPLEMENTED) \
    X(SOCKET_IS_NOT_CONNECTED) \
    X(NOT_A_DIRECTORY) \
    X(DIRECTORY_NOT_EMPTY) \
    X(SOCKET_OPERATION_ON_NON_SOCKET) \
    X(OPERATION_NOT_SUPPORTED_ON_SOCKET) \
    X(OPERATION_NOT_PERMITTED) \
    /*DOC: need to describe difference between BROKEN_PIPE and CONNECTION_RESET_BY_PEER*/ \
    /*BROKEN_PIPE*/ \
    X(OPERATION_ON_CLOSED_SOCKET) \
    X(PROTOCOL_ERROR) \
    X(PROTOCOL_NOT_SUPPORTED) \
    X(PROTOCOL_WRONG_TYPE_FOR_SOCKET) \
    X(RESULT_TOO_LARGE) \
    X(READ_ONLY_FILE_SYSTEM) \
    X(CANNOT_SEND_AFTER_TRANSPORT_ENDPOINT_SHUTDOWN) \
    X(INVALID_SEEK) \
    X(NO_SUCH_PROCESS) \
    X(CONNECTION_TIMED_OUT) \
    X(TEXT_FILE_IS_BUSY) \
    X(CROSS_DEVICE_LINK_NOT_PERMITTED) \
    X(UNKNOWN_ERROR) \
    X(END_OF_FILE) \
    X(NO_SUCH_DEVICE_OR_ADDRESS) \
    X(TOO_MANY_LINKS) \
    X(HOST_IS_DOWN) \
    X(REMOTE_IO_ERROR) \
    X(INAPPROPRIATE_IOCTL_FOR_DEVICE) \
    X(INAPPROPRIATE_FILE_TYPE_OR_FORMAT)

#define X(PARAM) PARAM,
enum class StatusCode : uint32_t {
  TARM_IO_LIST_OF_STATUS_CODES
  FIRST = OK,
  LAST = INAPPROPRIATE_FILE_TYPE_OR_FORMAT
};
#undef X

StatusCode convert_from_uv(std::int64_t libuv_code);

TARM_IO_DLL_PUBLIC
std::ostream& operator<<(std::ostream& out, StatusCode code);

} // namespace io
} // namespace tarm

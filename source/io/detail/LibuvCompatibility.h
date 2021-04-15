/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include <uv.h>

// Exctracted from uv-common.h which is not exported.
// This constant is used by uv_handle flags
enum {
    TARM_IO_UV_HANDLE_BOUND =        0x00002000,
    TARM_IO_UV_HANDLE_SHUTTING =     0x00000100,
    TARM_IO_UV_HANDLE_READ_PENDING = 0x00010000,
};


// Reimplementation of uv_tcp_close_reset for old versions of libuv
#if !(UV_VERSION_MAJOR >= 1 && UV_VERSION_MINOR >= 32 && UV_VERSION_PATCH >= 0)
int uv_tcp_close_reset(uv_tcp_t* handle, uv_close_cb close_cb);
#endif

// Reimplementation of internal uv__socket_sockopt.
// Supports TCP, UDP and named pipe.
// if value == 0, acts as getter, otherwise as setter
int tarm_io_socket_option(uv_handle_t* handle, int optname, int* value);

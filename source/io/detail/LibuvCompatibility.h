/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include <uv.h>

// Exctracted from uv-common.h which is not exported.
// This constant is used by uv_handle flags
enum {
    IO_UV_HANDLE_BOUND = 0x00002000,
    IO_UV_HANDLE_SHUTTING = 0x00000100
};

// Reimplementation of uv_tcp_close_reset for old versions of libuv
// TODO: check libuv VERSION
int uv_tcp_close_reset(uv_tcp_t* handle, uv_close_cb close_cb);

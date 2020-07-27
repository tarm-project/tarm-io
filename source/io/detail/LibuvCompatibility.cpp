/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "LibuvCompatibility.h"

// TODO: check libuv VERSION
#ifdef TARM_IO_PLATFORM_WINDOWS
int uv_tcp_close_reset(uv_tcp_t* handle, uv_close_cb close_cb) {
    struct linger l = { 1, 0 };

    // Disallow setting SO_LINGER to zero due to some platform inconsistencies
    if (handle->flags & IO_UV_HANDLE_SHUTTING) {
        return UV_EINVAL;
    }

    if (0 != setsockopt(handle->socket, SOL_SOCKET, SO_LINGER, (const char*)&l, sizeof(l))) {
        return uv_translate_sys_error(WSAGetLastError());
    }

    uv_close(reinterpret_cast<uv_handle_t*>(handle), close_cb);

    return 0;
}
#else
int uv_tcp_close_reset(uv_tcp_t* handle, uv_close_cb close_cb) {
    struct linger l = { 1, 0 };

    // Disallow setting SO_LINGER to zero due to some platform inconsistencies
    if (handle->flags & IO_UV_HANDLE_SHUTTING) {
        return UV_EINVAL;
    }

    uv_os_fd_t native_handle;
    int status = uv_fileno(reinterpret_cast<uv_handle_t*>(handle), &native_handle);
    if (status < 0) {
        return status;
    }

    if (setsockopt(native_handle, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) != 0) {
        return uv_translate_sys_error(errno);
    }

    uv_close(reinterpret_cast<uv_handle_t*>(handle), close_cb);
    return 0;
}
#endif

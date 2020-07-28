/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "LibuvCompatibility.h"

#if !(UV_VERSION_MAJOR >= 1 && UV_VERSION_MINOR >= 32 && UV_VERSION_PATCH >= 0)
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
#endif

int tarm_io_socket_option(uv_handle_t* handle, int optname, int* value) {
    if (handle == NULL || value == NULL) {
      return UV_EINVAL;
    }

#ifdef TARM_IO_PLATFORM_WINDOWS
    SOCKET socket = 0;

    if (handle->type == UV_TCP) {
        socket = reinterpret_cast<uv_tcp_t*>(handle)->socket;
    } else if (handle->type == UV_UDP) {
        socket = reinterpret_cast<uv_udp_t*>(handle)->socket;
    } else {
        return UV_ENOTSUP;
    }

    int len = sizeof(*value);
    int r = 0;
    if (*value == 0) {
        r = getsockopt(socket, SOL_SOCKET, optname, reinterpret_cast<char*>(value), &len);
    } else {
        r = setsockopt(socket, SOL_SOCKET, optname, reinterpret_cast<const char*>(value), len);
    }

    if (r == SOCKET_ERROR) {
        return uv_translate_sys_error(::WSAGetLastError());
    }
#else
    uv_os_fd_t native_handle = 0;
    if (handle->type == UV_TCP || handle->type == UV_NAMED_PIPE) {
        int status = uv_fileno(reinterpret_cast<uv_handle_t*>(handle), &native_handle);
        if (status < 0) {
            return status;
        }
    } else if (handle->type == UV_UDP) {
        native_handle = reinterpret_cast<uv_udp_t*>(handle)->io_watcher.fd;
    } else {
        return UV_ENOTSUP;
    }

    socklen_t len = sizeof(*value);
    int r = 0;
    if (*value == 0) {
        r = getsockopt(native_handle, SOL_SOCKET, optname, value, &len);
    } else {
        r = setsockopt(native_handle, SOL_SOCKET, optname, reinterpret_cast<const void*>(value), len);
    }

    if (r < 0) {
        return uv_translate_sys_error(errno);
    }
#endif

    return 0;
}

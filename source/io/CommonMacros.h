/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "Export.h"

#define IO_FORBID_COPY(X)            \
    X(const X&) = delete;            \
    X& operator=(const X&) = delete;

#define IO_FORBID_MOVE(X)             \
    X(const X&&) = delete;            \
    X& operator=(const X&&) = delete;

#define IO_ALLOW_COPY(X)              \
    X(const X&) = default;            \
    X& operator=(const X&) = default;

#define IO_ALLOW_MOVE(X)         \
    X(X&&) = default;            \
    X& operator=(X&&) = default;

#define IO_DECLARE_MOVE(X) \
    X(X&&);                \
    X& operator=(X&&);

#define IO_DECLARE_DLL_PUBLIC_MOVE(X) \
    IO_DLL_PUBLIC X(X&&);                \
    IO_DLL_PUBLIC X& operator=(X&&);

#define IO_DEFINE_DEFAULT_MOVE(X)   \
    X::X(X&&) = default;            \
    X& X::operator=(X&&) = default;

#define IO_MAKE_LOGGED_ERROR(LOOP, ADDRESS, STATUS_CODE, MESSAGE) \
    (IO_LOG(LOOP, ERROR, ADDRESS, MESSAGE)),              \
    ::io::Error(STATUS_CODE, MESSAGE)

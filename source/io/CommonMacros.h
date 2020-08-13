/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "Export.h"

#define TARM_IO_STRINGIFY(V) TARM_IO_STRINGIFY_HELPER(V)
#define TARM_IO_STRINGIFY_HELPER(V) #V

#define TARM_IO_FORBID_COPY(X)       \
    X(const X&) = delete;            \
    X& operator=(const X&) = delete;

#define TARM_IO_FORBID_MOVE(X)        \
    X(const X&&) = delete;            \
    X& operator=(const X&&) = delete;

#define TARM_IO_ALLOW_COPY(X)         \
    X(const X&) = default;            \
    X& operator=(const X&) = default;

#define TARM_IO_ALLOW_MOVE(X)     \
    X(X&&) = default;             \
    X& operator=(X&&) = default;

#define TARM_IO_DECLARE_MOVE(X) \
    X(X&&);                     \
    X& operator=(X&&);

#define TARM_IO_DECLARE_DLL_PUBLIC_MOVE(X) \
    TARM_IO_DLL_PUBLIC X(X&&);             \
    TARM_IO_DLL_PUBLIC X& operator=(X&&);

#define TARM_IO_DEFINE_DEFAULT_MOVE(X) \
    X::X(X&&) = default;               \
    X& X::operator=(X&&) = default;

/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "../EventLoop.h"

#include <functional>
#include <utility>

namespace tarm {
namespace io {
namespace detail {

template<typename F, typename... Params>
void defer_execution_if_required(EventLoop& loop, F&& f) {
    if (loop.is_running()) {
        f();
    } else {
        loop.execute_on_loop_thread([=](io::EventLoop&) {
            f();
        });
    }
}

template<typename F, typename... Params>
void defer_execution_if_required(EventLoop& loop, F&& f, Params&&... params) {
    if (loop.is_running()) {
        f(std::forward<Params>(params)...);
    } else {
        auto foo = std::bind(std::forward<F>(f), std::forward<Params>(params)...);
        loop.execute_on_loop_thread(foo);
    }
}

template<typename F, typename... Params>
void defer_execution_if_required(EventLoop& loop, F&& f, EventLoop& loop_as_param, Params&&... params) {
    if (loop.is_running()) {
        f(loop_as_param, std::forward<Params>(params)...);
    } else {
        loop.execute_on_loop_thread(std::bind(std::forward<F>(f), std::placeholders::_1, std::forward<Params>(params)...));
    }
}


} // namespace detail
} // namespace io
} // namespace tarm

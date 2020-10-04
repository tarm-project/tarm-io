/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "Error.h"
#include "EventLoop.h"

namespace tarm {
namespace io {
namespace fs {
namespace detail {

template<typename T, typename OpenCallback>
bool open(EventLoop& loop, T& fs_object, const Path& path, const OpenCallback& callback) {
    if (fs_object.state() == T::State::OPENED) {
        fs_object.close([&fs_object, &loop, path, callback](typename T::ParentType& dir, const Error& error) {
            if (error) {
                if(callback) {
                    callback(dir, error);
                }
            } else {
                loop.schedule_callback([&fs_object, path, callback](EventLoop&) {
                    fs_object.open(path, callback);
                });
            }
        });
        return false;
    } else if (!(fs_object.state() == T::State::INITIAL || fs_object.state() == T::State::CLOSED)) {
        loop.schedule_callback([&fs_object, path, callback](EventLoop&) {
            fs_object.open(path, callback);
        });
        return false;
    }

    return true;
}

} // namespace detail
} // namespace fs
} // namespace io
} // namespace tarm

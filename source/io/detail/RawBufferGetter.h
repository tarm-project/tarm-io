/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include <memory>
#include <string>

namespace tarm {
namespace io {
namespace detail {

inline const char* raw_buffer_get(const char* pc) {
    return pc;
}

inline const char* raw_buffer_get(const std::string& s) {
    return s.c_str();
}

inline const char* raw_buffer_get(const std::shared_ptr<const char>& p) {
    return p.get();
}

} // namespace detail
} // namespace io
} // namespace tarm


/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "Export.h"
#include "CommonMacros.h"

#include <memory>

namespace tarm {
namespace io {

class UserDataHolder {
public:
    TARM_IO_FORBID_COPY(UserDataHolder);
    TARM_IO_FORBID_MOVE(UserDataHolder);

    TARM_IO_DLL_PUBLIC UserDataHolder();
    TARM_IO_DLL_PUBLIC ~UserDataHolder();

    TARM_IO_DLL_PUBLIC void set_user_data(void* data);
    TARM_IO_DLL_PUBLIC void* user_data();

    template<typename T>
    T* user_data_as_ptr() {
        return reinterpret_cast<T*>(user_data());
    }

    template<typename T>
    T& user_data_as_ref() {
        return *reinterpret_cast<T*>(user_data());
    }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
} // namespace tarm

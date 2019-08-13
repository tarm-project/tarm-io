#pragma once

#include "Export.h"

#include <memory>

namespace io {

class UserDataHolder {
public:
    IO_DLL_PUBLIC UserDataHolder();
    IO_DLL_PUBLIC ~UserDataHolder();

    IO_DLL_PUBLIC void set_user_data(void* data);
    IO_DLL_PUBLIC void* user_data();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

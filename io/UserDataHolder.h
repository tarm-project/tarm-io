#pragma once

#include <memory>

namespace io {

class UserDataHolder {
public:
    UserDataHolder();
    ~UserDataHolder();

    void set_user_data(void* data);
    void* user_data();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io

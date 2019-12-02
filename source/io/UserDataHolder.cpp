#include "UserDataHolder.h"

namespace io {

class UserDataHolder::Impl {
public:
    Impl();
    void set_user_data(void* data);
    void* user_data();

private:
    void* m_user_data = nullptr;
};

UserDataHolder::Impl::Impl() :
    m_user_data(nullptr) {
}

void UserDataHolder::Impl::set_user_data(void* data) {
    m_user_data = data;
}

void* UserDataHolder::Impl::user_data() {
    return m_user_data;
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

UserDataHolder::UserDataHolder() :
    m_impl(new Impl) {
}

UserDataHolder::~UserDataHolder() = default;

// Need to define move constructor and assignment explicitly to allow PIMPL usage with unique_ptr
// The same is about destructor above
IO_DEFINE_DEFAULT_MOVE(UserDataHolder);

void UserDataHolder::set_user_data(void* data) {
    return m_impl->set_user_data(data);
}

void* UserDataHolder::user_data() {
    return m_impl->user_data();
}

} // namespace io

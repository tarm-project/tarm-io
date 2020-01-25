#include "RefCounted.h"

namespace io {

RefCounted::RefCounted(EventLoop& loop) :
    Removable(loop),
    m_ref_counter(0) {
    ref();
}

void RefCounted::ref() {
    ++m_ref_counter;
}

void RefCounted::unref() {
    if (!m_ref_counter) {
        return;
    }

    --m_ref_counter;

    if (m_ref_counter == 0) {
        this->schedule_removal();
    }
}

std::size_t RefCounted::ref_count() {
    return m_ref_counter;
}

} // namespace io

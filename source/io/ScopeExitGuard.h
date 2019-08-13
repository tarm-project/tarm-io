#pragma once

#include "Export.h"

#include <functional>

namespace io {

class ScopeExitGuard {
public:
    using Callback = std::function<void()>;

    ScopeExitGuard(Callback callback) : m_callback(callback) {
    }

    ~ScopeExitGuard() {
        if (m_callback) {
            m_callback();
        }
    }

    void reset() {
        m_callback = nullptr;
    }

private:
    Callback m_callback;
};

} // namespace io

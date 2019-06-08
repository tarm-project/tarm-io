#pragma once

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
private:
    Callback m_callback;
};

} // namespace io

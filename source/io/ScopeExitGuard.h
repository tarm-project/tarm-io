/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

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

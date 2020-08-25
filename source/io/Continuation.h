
/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"

namespace tarm {
namespace io {

class TARM_IO_DLL_PUBLIC Continuation {
public:
    void stop() {
        m_proceed = false;
    }

    bool proceed() const {
        return m_proceed;
    }

private:
    bool m_proceed = true;
};

} // namespace io
} // namespace tarm

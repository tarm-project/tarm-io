/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "Error.h"

#include <cstddef>

namespace io {

struct BufferSizeResult {
    BufferSizeResult(Error e, std::size_t s) :
        error(e),
        size(s) {
    }

    Error error;
    std::size_t size;
};

} // namespace io

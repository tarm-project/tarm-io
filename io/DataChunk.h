#pragma once

#include <memory>

namespace io {

struct DataChunk {
    DataChunk() = default;
    DataChunk(std::shared_ptr<const char> b, std::size_t s, std::size_t o) :
        buf(b),
        size(s),
        offset(o){
    }

    std::shared_ptr<const char> buf;
    std::size_t size = 0;
    std::size_t offset = 0;
};

} // namespace io

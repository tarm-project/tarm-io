#pragma once

#include <ostream>
#include <assert.h>

namespace io {

#define IO_LIST_OF_DIRECTORY_ENTRY_TYPES \
    X(UNKNOWN) \
    X(FILE) \
    X(DIR) \
    X(LINK) \
    X(FIFO) \
    X(SOCKET) \
    X(CHAR) \
    X(BLOCK)

#define X(PARAM) PARAM,
enum class DirectoryEntryType {
    IO_LIST_OF_DIRECTORY_ENTRY_TYPES
};
#undef X

inline std::ostream& operator<< (std::ostream& out, DirectoryEntryType type) {
    switch(type) {
#define X(PARAM) case DirectoryEntryType::PARAM: return out << #PARAM; break;
    IO_LIST_OF_DIRECTORY_ENTRY_TYPES
#undef X
    default:
        assert(false);
        return out << static_cast<std::uint64_t>(type);
    }
}


} // namespace io

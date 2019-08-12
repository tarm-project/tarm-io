#pragma once

namespace io {

enum class DirectoryEntryType {
    UNKNOWN = 0,
    FILE,
    DIR,
    LINK,
    FIFO,
    SOCKET,
    CHAR,
    BLOCK
};

inline
std::ostream& operator<< (std::ostream& os, DirectoryEntryType type) {
    switch (type) {
        case DirectoryEntryType::UNKNOWN:
            return os << "UNKNOWN";
        case DirectoryEntryType::FILE:
            return os << "FILE";
        case DirectoryEntryType::DIR:
            return os << "DIR";
        case DirectoryEntryType::LINK:
            return os << "LINK";
        case DirectoryEntryType::FIFO:
            return os << "FIFO";
        case DirectoryEntryType::SOCKET:
            return os << "SOCKET";
        case DirectoryEntryType::CHAR:
            return os << "CHAR";
        case DirectoryEntryType::BLOCK:
            return os << "BLOCK";
        // omit default case to trigger compiler warning for missing cases
    };
    return os << static_cast<std::uint64_t>(type);
}


} // namespace io

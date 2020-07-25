#include "Version.h"

#define TARM_IO_VERSION_STRING_BASE TARM_IO_STRINGIFY(TARM_IO_VERSION_MAJOR) "." \
                                    TARM_IO_STRINGIFY(TARM_IO_VERSION_MINOR) "." \
                                    TARM_IO_STRINGIFY(TARM_IO_VERSION_PATCH)

namespace tarm {
namespace io {

unsigned version_number(void) {
    return TARM_IO_VERSION_HEX;
}

std::string version_string(void) {
    return TARM_IO_VERSION_STRING_BASE;
}

} // namespace io
} // namespace tarm

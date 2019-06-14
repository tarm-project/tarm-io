#include "Configuration.h"

namespace io {
namespace global {

Logger::Callback g_logger_callback = nullptr;

Logger::Callback logger_callback() {
    return g_logger_callback;
}

void set_logger_callback(Logger::Callback callback) {
    g_logger_callback = callback;
}

} // namespace global
} // namespace io

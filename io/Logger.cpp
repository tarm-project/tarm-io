#include "Logger.h"

namespace io {

Logger::Logger() {

}

Logger::Logger(const std::string& prefix) :
    m_prefix(prefix) {
}

void Logger::enable_log(Callback callback) {
    m_callback = callback;
}

} // namespace io
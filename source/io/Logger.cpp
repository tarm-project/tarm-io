/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "Logger.h"

namespace tarm {
namespace io {



Logger::Logger() {

}

Logger::Logger(const std::string& prefix) :
    m_prefix(prefix) {
}

void Logger::enable_log(const Callback& callback) {
    m_callback = callback;
}

void Logger::out_common_prefix(std::ostream& ss, Logger::Severity severity) {
    ss << "[" << severity << "]";

    if (!m_prefix.empty()) {
        ss << " <" << m_prefix << ">";
    }
}

} // namespace io
} // namespace tarm

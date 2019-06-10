#pragma once

#include <ostream>
#include <functional>
#include <sstream>

namespace io {

class Logger {
public:
    enum class Severity {
        INFO,
        WARNING,
        ERROR
    };

    using Callback = std::function<void(const std::string& message)>;

    Logger();
    Logger(const std::string& prefix);

    void enable_log(Callback callback);

    template<typename... T>
    void log(Severity severity, T... t);

private:
    template<typename M, typename... T>
    void log(std::ostream& os, const M& message_chunk, T... t) {
        os << message_chunk;
        log(os, t...);
    }

    template<typename M>
    void log(std::ostream& os, const M& message_chunk) {
        os << message_chunk;
    }

    Callback m_callback = nullptr;
    std::string m_prefix;
};

inline
std::ostream& operator<< (std::ostream& os, Logger::Severity severity) {
    switch (severity) {
        case Logger::Severity::INFO:
            return os << "INFO";
        case Logger::Severity::WARNING:
            return os << "WARNING";
        case Logger::Severity::ERROR:
            return os << "ERROR";
    }

    return os << "?????";
}

///////////////////// IMPLEMENTATION /////////////////////
template<typename... T>
void Logger::log(Severity severity, T... t) {
    if (m_callback == nullptr) {
        return;
    }

    std::stringstream ss;

    ss << "[" << severity << "] ";

    if (!m_prefix.empty()) {
        ss << "<" << m_prefix << "> ";
    }

    log(ss, t...);

    m_callback(ss.str());
}

} // namespace io
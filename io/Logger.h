#pragma once

#include "detail/ConstexprString.h"

#include <ostream>
#include <functional>
#include <sstream>

#define IO_LOG(LOGGER_PTR, SEVERITY, ...) LOGGER_PTR->log_with_file_and_line(io::Logger::Severity::SEVERITY, ::io::detail::extract_file_name_from_path(__FILE__), __LINE__, __VA_ARGS__);

namespace io {

class Logger {
public:
    enum class Severity {
        TRACE,
        DEBUG,
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

    template<typename... T>
    void log_with_file_and_line(Severity severity, const char* const file, std::size_t line, T... t);

private:
    template<typename M, typename... T>
    void log(std::ostream& os, const M& message_chunk, T... t) {
        os << " " << message_chunk;
        log(os, t...);
    }

    template<typename M>
    void log(std::ostream& os, const M& message_chunk) {
        os << " " << message_chunk;
    }

    void out_common_prefix(std::ostream& ss, Logger::Severity severity);

    Callback m_callback = nullptr;
    std::string m_prefix;
};

inline
std::ostream& operator<< (std::ostream& os, Logger::Severity severity) {
    switch (severity) {
        case Logger::Severity::TRACE:
            return os << "TRACE";
        case Logger::Severity::DEBUG:
            return os << "DEBUG";
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
    out_common_prefix(ss, severity);
    log(ss, t...);

    m_callback(ss.str());
}

template<typename... T>
void Logger::log_with_file_and_line(Severity severity, const char* const file, std::size_t line, T... t) {
    if (m_callback == nullptr) {
        return;
    }

    std::stringstream ss;
    out_common_prefix(ss, severity);
    ss << " [" << file << ":" << line << "]";
    log(ss, t...);

    m_callback(ss.str());
}

} // namespace io

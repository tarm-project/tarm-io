#pragma once

// TODO: remove
//#define LOG_SEVERITY(LOGGER_PTR, ...) { \
//    constexpr const char* file_name = ::tarm::io::detail::extract_file_name_from_path(__FILE__); \
//    (LOGGER_PTR)->log_with_compile_context(::tarm::io::Logger::Severity::SEVERITY, file_name, __LINE__, __func__, __VA_ARGS__);}

// No TARM_IO_ prefix because these macros are purely internal and not exposed
#define LOG_IMPL(LOGGER_PTR, SEVERITY, ...) { \
    constexpr const char* file_name = ::tarm::io::detail::extract_file_name_from_path(__FILE__); \
    (LOGGER_PTR)->log_with_compile_context(::tarm::io::Logger::Severity::SEVERITY, file_name, __LINE__, __func__, __VA_ARGS__);}

#define LOG_TRACE(LOGGER_PTR, ...) \
    LOG_IMPL((LOGGER_PTR), TRACE, __VA_ARGS__)

#define LOG_DEBUG(LOGGER_PTR, ...) \
    LOG_IMPL((LOGGER_PTR), DEBUG, __VA_ARGS__)

#define LOG_INFO(LOGGER_PTR, ...) \
    LOG_IMPL((LOGGER_PTR), INFO, __VA_ARGS__)

#define LOG_WARNING(LOGGER_PTR, ...) \
    LOG_IMPL((LOGGER_PTR), WARNING, __VA_ARGS__);

#define LOG_ERROR(LOGGER_PTR, ...) \
    LOG_IMPL((LOGGER_PTR), ERROR, __VA_ARGS__)

#pragma once

#define IO_LOG(LOGGER_PTR, SEVERITY, ...) { \
    constexpr const char* file_name = ::tarm::io::detail::extract_file_name_from_path(__FILE__); \
    (LOGGER_PTR)->log_with_compile_context(::tarm::io::Logger::Severity::SEVERITY, file_name, __LINE__, __func__, __VA_ARGS__);}

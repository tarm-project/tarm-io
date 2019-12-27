#pragma once

#include "io/global/Configuration.h"

#include <boost/filesystem/path.hpp>

#include <fstream>
#include <mutex>

extern const char* const TEST_OUTPUT_PATH_RELATIVE;

// LogRedirector class is used to redirect each test's logs into separate log files
class LogRedirector {
public:
    LogRedirector(const std::string& log_path = TEST_OUTPUT_PATH_RELATIVE) noexcept;
    void log_to_stdout();

private:
    boost::filesystem::path m_logs_directory;
    std::ofstream m_ofstream;
    std::mutex m_log_mutex;
};

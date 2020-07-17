/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "LogRedirector.h"

#include "UTCommon.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <assert.h>

const char* const TEST_OUTPUT_PATH_RELATIVE = "../testlogs";
const char* const LOG_EXTENSION = ".log";

LogRedirector::LogRedirector(const std::string& log_path) noexcept {
    const auto exec_dir = exe_path();
    m_logs_directory = exec_dir / log_path;

    if (!boost::filesystem::exists(m_logs_directory)) {
        boost::filesystem::create_directories(m_logs_directory);
    }

    const auto full_path = m_logs_directory / (current_test_suite_name() + "." + current_test_case_name() + LOG_EXTENSION);
    m_ofstream.open(full_path.string());
    assert(!m_ofstream.fail());

    io::global::set_logger_callback([this](const std::string& message) {
        std::lock_guard<decltype(m_log_mutex)> guard(m_log_mutex);
        m_ofstream << message << std::endl;
    });
}

void LogRedirector::log_to_stdout() {
    io::global::set_logger_callback([this](const std::string& message) {
        std::lock_guard<decltype(m_log_mutex)> guard(m_log_mutex);
        std::cout << message << std::endl;
    });
}

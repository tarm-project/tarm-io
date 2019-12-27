#include "LogRedirector.h"

#include <gtest/gtest.h>

#include <boost/dll.hpp>
#include <boost/filesystem.hpp>

#include <assert.h>

const char* const TEST_OUTPUT_PATH_RELATIVE = "../testlogs";
const char* const LOG_EXTENSION = ".log";

LogRedirector::LogRedirector(const std::string& log_path) noexcept {
    const auto exec_dir =  boost::dll::program_location().parent_path();
    m_logs_directory = exec_dir / log_path;

    if (!boost::filesystem::exists(m_logs_directory)) {
        boost::filesystem::create_directories(m_logs_directory);
    }

    const std::string current_test_scope = ::testing::UnitTest::GetInstance()->current_test_case()->name();
    const std::string current_test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();

    const auto full_path = m_logs_directory / (current_test_scope + "." + current_test_name + LOG_EXTENSION);
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
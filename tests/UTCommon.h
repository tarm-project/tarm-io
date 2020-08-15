/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "LogRedirector.h"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <chrono>

using namespace tarm;

#ifdef GTEST_SKIP
    #define TARM_IO_TEST_SKIP() GTEST_SKIP()
#else
    #define TARM_IO_TEST_SKIP() return
#endif

#ifdef TARM_IO_PLATFORM_WINDOWS
    #define TARM_IO_TEST_SKIP_ON_WINDOWS() TARM_IO_TEST_SKIP()
#else
    #define TARM_IO_TEST_SKIP_ON_WINDOWS() // do nothing
#endif


std::string create_temp_test_directory();
std::string create_empty_file(const std::string& path_where_create);

boost::filesystem::path exe_path();

std::string current_test_suite_name();
std::string current_test_case_name();

// std::chrono pretty printers for GTest
namespace std {

void PrintTo(const std::chrono::minutes& duration, std::ostream* os);
void PrintTo(const std::chrono::seconds& duration, std::ostream* os);
void PrintTo(const std::chrono::milliseconds& duration, std::ostream* os);
void PrintTo(const std::chrono::microseconds& duration, std::ostream* os);
void PrintTo(const std::chrono::nanoseconds& duration, std::ostream* os);

} // namespace std

#define EXPECT_IN_RANGE(VAL, MIN, MAX) \
    EXPECT_GE((VAL), (MIN));           \
    EXPECT_LE((VAL), (MAX))

#define ASSERT_IN_RANGE(VAL, MIN, MAX) \
    ASSERT_GE((VAL), (MIN));           \
    ASSERT_LE((VAL), (MAX))

#define EXPECT_TIMEOUT_MS(EXPECTED_MS, ACTUAL_MS)                     \
    EXPECT_IN_RANGE(std::chrono::milliseconds(ACTUAL_MS),             \
        std::chrono::milliseconds(std::int64_t(EXPECTED_MS * 0.9f)), \
        std::chrono::milliseconds(std::int64_t(EXPECTED_MS * 1.2f)))


#define ASSERT_TIMEOUT_MS(EXPECTED_MS, ACTUAL_MS)                     \
    ASSERT_IN_RANGE(std::chrono::milliseconds(ACTUAL_MS),             \
        std::chrono::milliseconds(std::int64_t(EXPECTED_MS * 0.9f)), \
        std::chrono::milliseconds(std::int64_t(EXPECTED_MS * 1.2f)))


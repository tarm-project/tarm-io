#pragma once

#include "LogRedirector.h"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#ifdef GTEST_SKIP
    #define IO_TEST_SKIP() GTEST_SKIP()
#else
    #define IO_TEST_SKIP() return
#endif

std::string create_temp_test_directory();

boost::filesystem::path exe_path();

/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

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

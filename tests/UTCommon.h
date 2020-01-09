#pragma once

#include "LogRedirector.h"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

std::string create_temp_test_directory();

boost::filesystem::path exe_path();
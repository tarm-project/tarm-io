#include "UTCommon.h"

#include <boost/filesystem.hpp>

#include <string>

std::string create_temp_test_directory() {
    auto path = boost::filesystem::temp_directory_path();
    path /= "uv_cpp";
    path /= "%%%%-%%%%-%%%%-%%%%-%%%%";
    path = boost::filesystem::unique_path(path);

    boost::filesystem::create_directories(path);

    return path.string();
}

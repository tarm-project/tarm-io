/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include <boost/filesystem.hpp>

#include <iostream>
#include <string>

std::string create_temp_test_directory() {
    auto path = boost::filesystem::temp_directory_path();
    path /= "uv_cpp";
    path /= "%%%%-%%%%-%%%%-%%%%-%%%%";
    path = boost::filesystem::unique_path(path);

    boost::filesystem::create_directories(path);

    return path.string();
}

#ifdef _WIN32
    #include <windows.h>    //GetModuleFileNameW
#else // assuming Unix
    #include <unistd.h>     //readlink

    #ifdef __APPLE__
        #include <sys/syslimits.h>
        #include <mach-o/dyld.h>
    #else
        #include <linux/limits.h>
    #endif
#endif

boost::filesystem::path exe_path() {
    boost::filesystem::path result;

#if defined(_WIN32)
    wchar_t path[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, path, MAX_PATH);
    result = boost::filesystem::path(path).remove_filename();
#elif defined(__APPLE__)
    char buf[PATH_MAX] = { 0 };
    uint32_t buf_size = PATH_MAX;
    if(!_NSGetExecutablePath(buf, &buf_size)) {
        result = boost::filesystem::path(std::string(buf, (buf_size > 0 ? buf_size : 0))).remove_filename();
    }
#else
    char buf[PATH_MAX];
    ssize_t buf_size = readlink("/proc/self/exe", buf, PATH_MAX);
    result = boost::filesystem::path(std::string(buf, (buf_size > 0 ? buf_size : 0))).remove_filename();
#endif

    if (result.empty()) {
        std::cerr << "Fatal error: failed to get executable path." << std::endl;
        abort();
    }

    return result;
}

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

#ifdef _WIN32
    #include <windows.h>    //GetModuleFileNameW
#else // assuming Unix
    #include <unistd.h>     //readlink

    #ifdef __APPLE__
        #include <sys/syslimits.h>
    #else
        #include <linux/limits.h>
    #endif
#endif

boost::filesystem::path exe_path() {
#ifdef _WIN32
    wchar_t path[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return boost::filesystem::path(path).remove_filename();
#else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    return boost::filesystem::path(std::string(result, (count > 0) ? count : 0)).remove_filename();
#endif
}

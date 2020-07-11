#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

message("================ Configuration summary ================")
# Helper macro to hide weak boolean type support in CMake
macro(tarm_io_config_summary_vars_set INPUT OUTPUT TRUE_TEXT FALSE_TEXT)
    if (${INPUT})
        set(${OUTPUT} ${TRUE_TEXT})
    else()
        set(${OUTPUT} ${FALSE_TEXT})
    endif()
endmacro()

tarm_io_config_summary_vars_set(TARM_IO_USE_EXTERNAL_LIBUV
                                TARM_IO_CONFIG_SUMMARY_BUNDLED_LIBUV
                                "NO"
                                "YES")

tarm_io_config_summary_vars_set(TARM_IO_USE_EXTERNAL_GTEST
                                TARM_IO_CONFIG_SUMMARY_BUNDLED_GTEST
                                "NO"
                                "YES")

tarm_io_config_summary_vars_set(TARM_IO_BUILD_TESTS
                                TARM_IO_CONFIG_SUMMARY_BUILD_TESTS
                                "YES"
                                "NO")

tarm_io_config_summary_vars_set(TARM_IO_OPENSSL_FOUND
                                TARM_IO_CONFIG_SUMMARY_OPENSSL
                                "YES"
                                "NO")

set(PLATFORM_STR "Undefined")
if (TARM_IO_PLATFORM_LINUX)
    set(PLATFORM_STR "Linux")
elseif (TARM_IO_PLATFORM_WINDOWS)
    set(PLATFORM_STR "Windows")
elseif (TARM_IO_PLATFORM_MACOSX)
    set(PLATFORM_STR "Mac OS")
endif()

message("Platform..................${PLATFORM_STR}")
message("OpenSSL support...........${TARM_IO_CONFIG_SUMMARY_OPENSSL}")
message("Bundled libuv.............${TARM_IO_CONFIG_SUMMARY_BUNDLED_LIBUV}")
message("Build tests...............${TARM_IO_CONFIG_SUMMARY_BUILD_TESTS}")
if (TARM_IO_BUILD_TESTS)
    message("Bundled GTest.............${TARM_IO_CONFIG_SUMMARY_BUNDLED_GTEST}")
endif()

if(TARM_IO_PLATFORM_MACOSX)
    message("OSX deployment target.....${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

message("=======================================================")
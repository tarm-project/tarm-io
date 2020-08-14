#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.5.0)

option(BUILD_SHARED_LIBS "Build tarm-io as shared library" ON)
option(TARM_IO_BUILD_TESTS "Build tests" OFF)
option(TARM_IO_USE_EXTERNAL_LIBUV "Use system libuv instead of the bundled one" OFF)
option(TARM_IO_USE_EXTERNAL_GTEST "Use system GTest instead of the bundled one" OFF)

set(TARM_IO_LIBUV_VERSION "1.30.1")
set(TARM_IO_GTEST_VERSION "1.10.0")

# According to docs this variable should be set before 'project' statement
if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.10" CACHE STRING "Minimum OS X deployment version")
endif()

project(tarm-io VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

if(UNIX AND NOT APPLE)
    message(STATUS "Platform is: Linux")
    set(TARM_IO_PLATFORM_LINUX TRUE)
endif()

if(WIN32)
    message(STATUS "Platform is: Windows")
    set(TARM_IO_PLATFORM_WINDOWS TRUE)
    if (MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP") # Multicore building
    endif()
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(TARM_IO_PLATFORM_MACOSX TRUE)
    message(STATUS "Platform is: Mac OS X")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
endif()

# IO_SANITIZER allows to build the library with sanitizer enabled
# For details see https://clang.llvm.org/docs/.
# value if set should be name of GCC/Clang sanitizers like 'thread', 'address', etc...
if(DEFINED IO_SANITIZER)
    message(STATUS "Using sanitizer: ${IO_SANITIZER}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=${IO_SANITIZER}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${IO_SANITIZER}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${IO_SANITIZER}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=${IO_SANITIZER}")
else()
    if (NOT MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
    endif()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if (TARM_IO_USE_EXTERNAL_LIBUV)
    find_package(LibUV REQUIRED)
endif()

add_subdirectory("thirdparty")
add_subdirectory("source")

if (TARM_IO_BUILD_TESTS)
    add_subdirectory(tests)
endif()

include(PrintConfigurationSummary )

#################################################################################################################
#                                         Debugging support scripts
#################################################################################################################

# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.5.0)

option(BUILD_SHARED_LIBS "Build tarm-io as shared library" ON)
option(TARM_IO_BUILD_TESTS "Build tests" OFF)
option(TARM_IO_USE_EXTERNAL_LIBUV "Use system libuv instead of the bundled one" OFF)

# TODO: enable only on MacOS X
# According to docs this variable should be set before 'project' statement
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version")
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

set(TARM_IO_LIBUV_VERSION "1.30.1")
set(TARM_IO_GTEST_VERSION "1.10.0")

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

#cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x64 -DCMAKE_GENERATOR_PLATFORM=x64 -DCMAKE_INSTALL_PREFIX=E:\projects\thirdparty\gtest\install ..\googletest-release-1.8.1
#cmake -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x64 -DCMAKE_GENERATOR_PLATFORM=x64 -DBOOST_ROOT="E:\projects\thirdparty\boost\install\include\boost-1_70" -DBOOST_LIBRARYDIR="E:\projects\thirdparty\boost\install\lib" -DBoost_USE_MULTITHREADED=ON -DBoost_USE_STATIC_RUNTIME=OFF -DGTEST_ROOT=E:\projects\thirdparty\gtest\install ..

# D:\uv_cpp\build>cmake -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x86 -DCMAKE_GENERATOR_PLATFORM=Win32 -DBOOST_ROOT=D:\boost_1_70_0 -DBoost_USE_MULTITHREADED=ON -DBoost_USE_STATIC_RUNTIME=OFF -DGTEST_ROOT=D:\gtest_install -DOPENSSL_ROOT_DIR=D:\Build-OpenSSL-VC-32\ ..

# ===================================================
# MSVC 2017, W7 virtual machine
# D:\tarm-io\build>cmake -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x64 -DCMAKE_GENERATOR_PLATFORM=x64 -DBOOST_ROOT=D:\boost_install -DBoost_USE_MULTITHREADED=ON -DBoost_USE_STATIC_RUNTIME=OFF -DGTEST_ROOT=D:\gtest_install  -DOPENSSL_ROOT_DIR=D:\openssl_install ..
#
# cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x64 -DCMAKE_GENERATOR_PLATFORM=x64 -DCMAKE_INSTALL_PREFIX=D:\gtest_install ..
# ===================================================

#32 bit
#gtest
#D:\googletest-release-1.10.0\build>cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x86 -DCMAKE_GENERATOR_PLATFORM=Win32 -DCMAKE_INSTALL_PREFIX=D:\gtest_install_32bit ..
#cmake --build . --config Debug

# Openssl build instructions
# https://github.com/my-devices/sdk/wiki/Building-OpenSSL-on-Windows

# boost
# b2 --build-dir=build/x86 address-model=32 threading=multi --build-type=complete --stagedir=./stage/x86 -j 3

#project
#cmake -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x86 -DCMAKE_GENERATOR_PLATFORM=Win32 -DBOOST_ROOT="F:\boost_install_32bit" -DBoost_USE_MULTITHREADED=ON -DBoost_USE_STATIC_RUNTIME=OFF -DGTEST_ROOT="D:\gtest_install_32bit" -DOPENSSL_ROOT_DIR=D:\openssl_install_32bit ..

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

message("================ Configuration summary ================")
message("OpenSSL support: ${TARM_IO_OPENSSL_FOUND}")
message("Build tests: ${TARM_IO_BUILD_TESTS}")
message("External libuv: ${TARM_IO_USE_EXTERNAL_LIBUV}")
message("=======================================================")

#############################################################
# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

cmake_minimum_required(VERSION 3.5.0)

option(BUILD_SHARED_LIBS "Build IO as shared library" ON)
option(IO_ENABLE_TESTS "Enable tests" ON)
option(IO_USE_EXTERNAL_LIBUV "Use system libuv instead of the bundled one" OFF)

# According to docs this variable should be set before 'project' statement
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version")
endif()

project(tarm_io VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

if(UNIX AND NOT APPLE)
    message(STATUS "Platform is: Linux")
    set(LINUX TRUE)
endif()

if(WIN32)
    message(STATUS "Platform is: Windows")
    if (MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP") # Multicore building
    endif()
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)
    message(STATUS "Platform is: Mac OS X")
endif()

if (NOT MSVC AND NOT DEFINED IO_SANITIZER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
endif()


if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
endif()

set(IO_LIBUV_VERSION "1.30.1")

#export TSAN_OPTIONS="halt_on_error=1"
if(DEFINED IO_SANITIZER)
    message(STATUS "Using sanitizer: ${IO_SANITIZER}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=${IO_SANITIZER}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${IO_SANITIZER}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${IO_SANITIZER}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=${IO_SANITIZER}")
endif()

#cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x64 -DCMAKE_GENERATOR_PLATFORM=x64 -DCMAKE_INSTALL_PREFIX=E:\projects\thirdparty\gtest\install ..\googletest-release-1.8.1
#cmake -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x64 -DCMAKE_GENERATOR_PLATFORM=x64 -DBOOST_ROOT="E:\projects\thirdparty\boost\install\include\boost-1_70" -DBOOST_LIBRARYDIR="E:\projects\thirdparty\boost\install\lib" -DBoost_USE_MULTITHREADED=ON -DBoost_USE_STATIC_RUNTIME=OFF -DGTEST_ROOT=E:\projects\thirdparty\gtest\install ..

# D:\uv_cpp\build>cmake -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x86 -DCMAKE_GENERATOR_PLATFORM=Win32 -DBOOST_ROOT=D:\boost_1_70_0 -DBoost_USE_MULTITHREADED=ON -DBoost_USE_STATIC_RUNTIME=OFF -DGTEST_ROOT=D:\gtest_install -DOPENSSL_ROOT_DIR=D:\Build-OpenSSL-VC-32\ ..

#find_package(PkgConfig REQUIRED)
#pkg_check_modules(LIBUV REQUIRED libuv)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if (IO_USE_EXTERNAL_LIBUV)
    find_package(LibUV REQUIRED)
endif()

add_subdirectory("thirdparty")
add_subdirectory("source")
add_subdirectory("samples")

if (IO_ENABLE_TESTS)
    add_subdirectory(tests)
endif()

message("================ Configuration summary ================")
message("IO_OPENSSL_FOUND ${IO_OPENSSL_FOUND}")

#############################################################
# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

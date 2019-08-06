cmake_minimum_required(VERSION 3.5.0)

project(connection_test
        VERSION 1.0.0
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)
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

# TODO: need to put these ifs elsewhere
if(WIN32)
  list(APPEND UV_LIB_DEPS
       advapi32
       iphlpapi
       psapi
       shell32
       user32
       userenv
       ws2_32)
else()
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
        # Android has pthread as part of its c library, not as a separate
        # libpthread.so.
        list(APPEND UV_LIB_DEPS pthread)
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "AIX")
    list(APPEND UV_LIB_DEPS perfstat)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    list(APPEND uv_libs dl)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "DragonFly|FreeBSD|NetBSD|OpenBSD")
    list(APPEND UV_LIB_DEPS kvm)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND UV_LIB_DEPS dl rt)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    list(APPEND UV_LIB_DEPS kstat nsl sendfile socket)
endif()

set(IO_LIBUV_ADDITIONAL_FLAGS "")
if (DEFINED IO_SANITIZER)
    set(IO_LIBUV_ADDITIONAL_FLAGS "${IO_LIBUV_ADDITIONAL_FLAGS} -fsanitize=${IO_SANITIZER}")
endif()

set(LIBUV_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/libuv")
set(LIBUV_STATIC_LIB_PATH "${LIBUV_PREFIX}/install/lib/libuv_a${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(LIBUV_DYNAMIC_LIB_PATH "${LIBUV_PREFIX}/install/lib/libuv${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(LIBUV_INCLUDE_PATH "${LIBUV_PREFIX}/install/include")
ExternalProject_Add(libuv
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libuv-${IO_LIBUV_VERSION}"
    PREFIX "${LIBUV_PREFIX}"
    BINARY_DIR "${LIBUV_PREFIX}/build"
    INSTALL_DIR "${LIBUV_PREFIX}/install"
    CMAKE_ARGS -Dlibuv_buildtests=OFF
               -DCMAKE_INSTALL_PREFIX=${LIBUV_PREFIX}/install
               -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
               -DCMAKE_C_FLAGS=-fPIC\ ${IO_LIBUV_ADDITIONAL_FLAGS}
               -DCMAKE_SHARED_LINKER_FLAGS=${IO_LIBUV_ADDITIONAL_FLAGS}
)
include_directories(${LIBUV_INCLUDE_PATH})

# TODO: system and filesystem should be required if we are building tests only
set(Boost_NO_BOOST_CMAKE ON)
find_package(Boost COMPONENTS system filesystem REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# TODO: need to find better name than io
# TODO: looks like need to move io-related CMake code into io folder
FILE(GLOB IO_SOURCE_LIST
        io/*.h
        io/global/*.h
        io/detail/*.h)
list(APPEND IO_SOURCE_LIST
        io/global/Configuration.cpp
        io/Common.cpp
        io/EventLoop.cpp
        io/Disposable.cpp
        io/Dir.cpp
        io/File.cpp
        io/Logger.cpp
        io/Status.cpp
        io/StatusCode.cpp
        io/Timer.cpp
        io/TcpClient.cpp
        io/TcpConnectedClient.cpp
        io/TcpServer.cpp
        io/UdpClient.cpp
        io/UdpClient.h
        io/UdpServer.h
        io/UdpServer.cpp
        io/UserDataHolder.cpp
)

if (${CMAKE_VERSION} VERSION_LESS "3.8.2")
    source_group(io FILES ${IO_SOURCE_LIST})
else()
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${IO_SOURCE_LIST})
endif()

add_library(io SHARED ${IO_SOURCE_LIST})
if (DEFINED IO_SANITIZER)
    target_link_libraries(io ${LIBUV_DYNAMIC_LIB_PATH})
else()
    target_link_libraries(io ${LIBUV_STATIC_LIB_PATH})
endif()
add_dependencies(io libuv)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${LIBUV_INCLUDE_DIR})

list(APPEND COMMON_LIB_LIST io)
list(APPEND COMMON_LIB_LIST ${LIBUV_STATIC_LIB_PATH})
list(APPEND COMMON_LIB_LIST ${UV_LIB_DEPS})

# TODO: put client and server into examples folder
add_executable(client Client.cpp)
target_link_libraries(client ${COMMON_LIB_LIST})

add_executable(server Server.cpp)
target_link_libraries(server ${COMMON_LIB_LIST})

set(BUILD_WITH_TESTS ON)
if (BUILD_WITH_TESTS)
    add_subdirectory(tests)

    if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
        include(CodeCoverage)
        APPEND_COVERAGE_COMPILER_FLAGS()
        set(COVERAGE_EXCLUDES
            '*/include/*/bits/*'
            '*/include/boost/*'
            '*/include/*/c++/*'
            '*/include/c++/*'
            '*/include/node/*'
            '*/include/gtest/*'
            'tests/*'
        )

        SETUP_TARGET_FOR_COVERAGE(
            NAME       coverage
            EXECUTABLE tests
        )
    endif()
endif()

#############################################################
# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

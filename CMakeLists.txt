cmake_minimum_required(VERSION 3.5.0)

project(connection_test
        VERSION 1.0.0
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
endif()

#export TSAN_OPTIONS="halt_on_error=1"
set(IO_SANITIZER "address")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=${IO_SANITIZER}")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${IO_SANITIZER}")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${IO_SANITIZER}")
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=${IO_SANITIZER}")

if (UNIX AND NOT MACOSX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUV REQUIRED libuv)
elseif(MACOSX)
    find_library(LIBUV_LIBRARIES uv PATHS /usr/local/lib)
    find_path(LIBUV_INCLUDE_DIR uv.h PATHS /usr/local/include/)
endif()

# TODO: need to check it if Boost installed to some location as /opt,
# because below I do not have add include dirs and libs
# TODO: system and filesystem should be required if we are building tests
find_package(Boost COMPONENTS system filesystem REQUIRED)

# TODO: need to find better name than io
# TODO: looks like need to move io-related CMake code into io folder
FILE(GLOB IO_SOURCE_LIST
        io/*.h
        io/global/*.h)
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
        io/UdpClient.cpp
        io/UdpClient.h
        io/UdpServer.h
        io/UdpServer.cpp
        io/Timer.cpp
        io/TcpClient.cpp
        io/TcpServer.cpp
)

if (${CMAKE_VERSION} VERSION_LESS "3.8.2")
    source_group(io FILES ${IO_SOURCE_LIST})
else()
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${IO_SOURCE_LIST})
endif()

set(IO_LIB_LIST)
list(APPEND IO_LIB_LIST ${LIBUV_LIBRARIES} ${LIBUV_LDFLAGS})

add_library(io ${IO_SOURCE_LIST})
target_link_libraries(io ${IO_LIB_LIST})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${LIBUV_INCLUDE_DIR})

# TODO: put client and server into examples folder
set(CLIENT_LIB_LIST io)
add_executable(client Client.cpp)
target_link_libraries(client ${CLIENT_LIB_LIST})

set(SERVER_LIB_LIST io)
add_executable(server Server.cpp)
target_link_libraries(server ${SERVER_LIB_LIST})

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

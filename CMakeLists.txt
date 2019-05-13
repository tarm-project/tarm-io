cmake_minimum_required(VERSION 3.5.0)

project(connection_test
        VERSION 1.0.0
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
endif()

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUV REQUIRED libuv)

# TODO: need to check it if Boost installed to some location as /opt,
# because below I do not have add include dirs and libs
find_package(Boost REQUIRED)

#message(FATAL_ERROR !${Boost_LIBRARIES}!)

set(SOURCE_LIST)
list(APPEND SOURCE_LIST io/Common.cpp)
list(APPEND SOURCE_LIST io/EventLoop.cpp)
list(APPEND SOURCE_LIST io/File.cpp)
list(APPEND SOURCE_LIST io/Timer.cpp)
list(APPEND SOURCE_LIST io/TcpClient.cpp)
list(APPEND SOURCE_LIST io/TcpServer.cpp)

set(LIB_LIST)
list(APPEND LIB_LIST ${LIBUV_LIBRARIES})
#message(FATAL_ERROR "!${LIBUV_INCLUDE_DIR}!")
include_directories(${LIBUV_INCLUDE_DIR})

add_executable(client Client.cpp ${SOURCE_LIST})
target_link_libraries(client ${LIB_LIST})

add_executable(server Server.cpp ${SOURCE_LIST})
target_link_libraries(server ${LIB_LIST})

#############################################################
# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()
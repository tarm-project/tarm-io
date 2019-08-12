cmake_minimum_required(VERSION 3.5.0)

project(connection_test VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

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

#cmake -DCMAKE_GENERATOR_PLATFORM=x64 -DBOOST_ROOT="E:\projects\thirdparty\boost\install\include\boost-1_70" -DBOOST_LIBRARYDIR="E:\projects\thirdparty\boost\install\lib" -DBoost_USE_MULTITHREADED=ON -DBoost_USE_STATIC_RUNTIME=OFF -DGTEST_ROOT=E:\projects\thirdparty\gtest\install ..

# TODO: system and filesystem should be required if we are building tests only
if (APPLE)
   set(BUILD_SHARED_LIBS ON)
   set(Boost_USE_STATIC_LIBS OFF)
elseif (WIN32)
   set(Boost_USE_STATIC_LIBS ON) # TODO: need to make this optional
endif()
find_package(Boost COMPONENTS system filesystem REQUIRED)

# TODO: investigate this for old versions of boost
#if(NOT TARGET Boost::filesystem)
#    add_library(Boost::filesystem IMPORTED INTERFACE)
#    set_property(TARGET Boost::filesystem PROPERTY
#        INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
#    set_property(TARGET Boost::filesystem PROPERTY
#        INTERFACE_LINK_LIBRARIES ${Boost_LIBRARIES})
#endif()

add_subdirectory("thirdparty")
add_subdirectory("source")

#list(APPEND COMMON_LIB_LIST io)
#list(APPEND COMMON_LIB_LIST ${LIBUV_STATIC_LIB_PATH})
#list(APPEND COMMON_LIB_LIST ${UV_LIB_DEPS})

# TODO: put client and server into examples folder
#add_executable(client Client.cpp)
#target_link_libraries(client ${COMMON_LIB_LIST})
#
#add_executable(server Server.cpp)
#target_link_libraries(server ${COMMON_LIB_LIST})

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

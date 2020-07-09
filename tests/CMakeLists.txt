#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

message(STATUS "Setting up tests.")

include(ProcessorCount)
ProcessorCount(N)

if (N EQUAL 0)
    set(N 1)
endif()
message(STATUS "Processors count: ${N}")

# Gtest is built during CMake configuration step because it is very lightweight
# and we want to be able to find it via 'find_package' which is impossible with ExternalProject_Add.
message(STATUS "Building bundled GTest")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gtest_build")

if (CMAKE_CONFIGURATION_TYPES)
    set(TARM_IO_MSVC_FLAGS "")
    if (MSVC)
        # -DBUILD_SHARED_LIBS=ON
        set(TARM_IO_MSVC_FLAGS -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=${CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE}
                               -DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
                               -Dgtest_force_shared_crt=ON)
    endif()

    execute_process(
        COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"
            ${TARM_IO_MSVC_FLAGS}
            -DCMAKE_INSTALL_PREFIX='${CMAKE_CURRENT_BINARY_DIR}/gtest_install'
            "${PROJECT_SOURCE_DIR}/thirdparty/googletest-1.10.0"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gtest_build")
    if(result)
        message(FATAL_ERROR "CMake step for googletest failed: ${output} \nresult: ${result}")
    endif()

    foreach(config_type Debug;Release)
        message("${config_type}")

        execute_process(COMMAND ${CMAKE_COMMAND} --build . -j ${N} --config "${config_type}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gtest_build")
        if(result)
            message(FATAL_ERROR "Build step for googletest failed: ${output} \nresult: ${result}")
        endif()

        execute_process(COMMAND ${CMAKE_COMMAND} --build . --target install --config "${config_type}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gtest_build")
        if(result)
            message(FATAL_ERROR "Install step for googletest failed: ${output} \nresult: ${result}")
        endif()

        message("${config_type} - done")
    endforeach()
else()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX='${CMAKE_CURRENT_BINARY_DIR}/gtest_install'
            "${PROJECT_SOURCE_DIR}/thirdparty/googletest-1.10.0"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gtest_build")
    if(result)
        message(FATAL_ERROR "CMake step for googletest failed: ${output} \nresult: ${result}")
    endif()

    execute_process(COMMAND ${CMAKE_COMMAND} --build . -j ${N}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gtest_build")
    if(result)
        message(FATAL_ERROR "Build step for googletest failed: ${output} \nresult: ${result}")
    endif()

    execute_process(COMMAND ${CMAKE_COMMAND} --build . --target install
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gtest_build")
    if(result)
        message(FATAL_ERROR "Install step for googletest failed: ${output} \nresult: ${result}")
    endif()
endif()

message(STATUS "Building bundled GTest - done")

find_package(GTest REQUIRED
             PATHS "${CMAKE_CURRENT_BINARY_DIR}/gtest_install"
             CONFIG
             NO_DEFAULT_PATH)
message(STATUS "Found GTest ${GTest_VERSION} target config in ${GTest_DIR}.")

if (WIN32)
   set(Boost_USE_STATIC_LIBS ON) # TODO: need to make this optional
endif()
#set(Boost_DEBUG ON)
find_package(Boost COMPONENTS system filesystem REQUIRED)

# Boost build withoud CMake targets (creating them manually)
if(NOT TARGET Boost::headers)
    add_library(Boost::headers IMPORTED INTERFACE GLOBAL)
    set_property(TARGET Boost::headers PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
endif()

if(NOT TARGET Boost::system)
    add_library(Boost::system IMPORTED INTERFACE GLOBAL)
    set_property(TARGET Boost::system PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
    set_property(TARGET Boost::system PROPERTY INTERFACE_LINK_LIBRARIES ${Boost_SYSTEM_LIBRARY})
endif()

if(NOT TARGET Boost::filesystem)
    add_library(Boost::filesystem IMPORTED INTERFACE GLOBAL)
    set_property(TARGET Boost::filesystem PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
    set_property(TARGET Boost::filesystem PROPERTY INTERFACE_LINK_LIBRARIES ${Boost_FILESYSTEM_LIBRARY})
endif()

# Note: tests are ordered from basic ones to advanced ones to allow check them from
# top to bottom if multiple tests fail.
FILE(GLOB TESTS_LIST *.h)
list(APPEND TESTS_LIST
    Main.cpp
    UTCommon.cpp
    LogRedirector.cpp
    ConstexprStringTest.cpp
    ByteSwapTest.cpp
    ErrorTest.cpp
    ConvertTest.cpp
    UserDataHolderTest.cpp
    LoggerTest.cpp
    RemovableTest.cpp
    RefCountedTest.cpp
    ScopeExitGuardTest.cpp
    VersionTest.cpp
    ConfigurationTest.cpp
    PathTest.cpp
    EndpointTest.cpp
    EventLoopTest.cpp
    TimerTest.cpp
    BacklogWithTimeoutTest.cpp
    FileTest.cpp
    DirTest.cpp
    UdpClientServerTest.cpp
    TcpClientServerTest.cpp
    TlsTcpClientServerTest.cpp
    DtlsClientServerTest.cpp
)

if (NOT DEFINED TARM_IO_OPENSSL_FOUND)
    message(FATAL_ERROR "IO CMake sctipts internal error, TARM_IO_OPENSSL_FOUND is not defined.")
endif()

if (NOT TARM_IO_OPENSSL_FOUND)
    message("OpenSSL is not found! Configuring tests without secure protocols.")
    list(FILTER TESTS_LIST EXCLUDE REGEX Tls.*)
    list(FILTER TESTS_LIST EXCLUDE REGEX Dtls.*)
    list(FILTER TESTS_LIST EXCLUDE REGEX VersionTest\.cpp)
endif()

set(RUN_TEST_ENV) # currently this is unused but may be utilized with sanitizers
set(TESTS_EXE_NAME tests)

add_executable(${TESTS_EXE_NAME} ${TESTS_LIST})
target_link_libraries(${TESTS_EXE_NAME}
    LibUV::LibUV
    tarm-io
    GTest::gtest
    Boost::system
    Boost::filesystem
)
add_dependencies(${TESTS_EXE_NAME} tarm-io)

if (OPENSSL_ROOT_DIR)
    target_include_directories(${TESTS_EXE_NAME} PUBLIC ${OPENSSL_ROOT_DIR}/include)
endif()

if (MSVC)
    target_compile_definitions(${TESTS_EXE_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# TODO: need to improve this somehow
if (${CMAKE_GENERATOR} MATCHES "Unix Makefiles")
    add_custom_command(TARGET ${TESTS_EXE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/data
                ${CMAKE_BINARY_DIR}/tests
    )
else()
    add_custom_command(TARGET ${TESTS_EXE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/data
                ${CMAKE_BINARY_DIR}/tests/$<$<OR:$<PLATFORM_ID:Windows>,$<PLATFORM_ID:Darwin>>:$<CONFIG>>
    )

    if (MSVC AND TARM_IO_OPENSSL_FOUND)
        # Copy openssl dlls to the tests folder
        add_custom_command(TARGET ${TESTS_EXE_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                    "${TARM_IO_SSL_EAY_DLL_PATH}"
                    "${CMAKE_BINARY_DIR}/tests/$<CONFIG>"
            COMMAND ${CMAKE_COMMAND} -E copy
                    "${TARM_IO_LIB_EAY_DLL_PATH}"
                    "${CMAKE_BINARY_DIR}/tests/$<CONFIG>"
            )
    endif()
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
    set(COVERAGE_EXCLUDES
        '*/include/*/bits/*'
        '*/include/boost/*'
        '*/include/*/c++/*'
        '*/include/c++/*'
        '*/include/gtest/*'
        '*/samples/*'
        '*/tests/*'
    )

    SETUP_TARGET_FOR_COVERAGE(
        NAME       coverage
        EXECUTABLE tests
    )
endif()

#add_custom_target(printFooKeys
#  COMMAND ${CMAKE_COMMAND} -E echo $<CONFIG>
#)

get_target_property(TARM_IO_GTEST_LIBRARY_TYPE GTest::gtest TYPE)
if(WIN32 AND NOT TARM_IO_GTEST_LIBRARY_TYPE STREQUAL "STATIC_LIBRARY")
    add_custom_command(TARGET ${TESTS_EXE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>/../bin/gtest$<$<CONFIG:DEBUG>:d>.dll
            $<TARGET_FILE_DIR:tests> # TODO: replace hardcoded 'tests' with ${TESTS_EXE_NAME}
    )
endif()

add_custom_target(RunTests
    DEPENDS ${TESTS_LIST}
    COMMAND ${CMAKE_COMMAND} -E env ${RUN_TEST_ENV} ./${TESTS_EXE_NAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_dependencies(RunTests ${TESTS_EXE_NAME})

# Get all propreties that cmake supports
execute_process(COMMAND cmake --help-property-list OUTPUT_VARIABLE CMAKE_PROPERTY_LIST)

# Convert command output into a CMake list
STRING(REGEX REPLACE ";" "\\\\;" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")
STRING(REGEX REPLACE "\n" ";" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")

function(print_properties)
    message ("CMAKE_PROPERTY_LIST = ${CMAKE_PROPERTY_LIST}")
endfunction(print_properties)

function(print_target_properties tgt)
    if(NOT TARGET ${tgt})
      message("There is no target named '${tgt}'")
      return()
    endif()

    foreach (prop ${CMAKE_PROPERTY_LIST})
        string(REPLACE "<CONFIG>" "${CMAKE_BUILD_TYPE}" prop ${prop})
    # Fix https://stackoverflow.com/questions/32197663/how-can-i-remove-the-the-location-property-may-not-be-read-from-target-error-i
    if(prop STREQUAL "LOCATION" OR prop MATCHES "^LOCATION_" OR prop MATCHES "_LOCATION$")
        continue()
    endif()
        # message ("Checking ${prop}")
        get_property(propval TARGET ${tgt} PROPERTY ${prop} SET)
        if (propval)
            get_target_property(propval ${tgt} ${prop})
            message ("${tgt} ${prop} = ${propval}")
        endif()
    endforeach(prop)
endfunction(print_target_properties)

print_target_properties(GTest::gtest)

# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

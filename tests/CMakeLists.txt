#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

message(STATUS "Searching for GTest")
find_package(GTest REQUIRED CONFIG HINTS ${GTEST_ROOT})
message(STATUS "Found GTest target config in ${GTest_DIR}.")

#cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x64 -DCMAKE_GENERATOR_PLATFORM=x64 -DCMAKE_INSTALL_PREFIX=E:\projects\thirdparty\gtest\install ..\googletest-release-1.8.1
#cmake -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x64 -DCMAKE_GENERATOR_PLATFORM=x64 -DBOOST_ROOT="E:\projects\thirdparty\boost\install\include\boost-1_70" -DBOOST_LIBRARYDIR="E:\projects\thirdparty\boost\install\lib" -DBoost_USE_MULTITHREADED=ON -DBoost_USE_STATIC_RUNTIME=OFF -DGTEST_ROOT=E:\projects\thirdparty\gtest\install ..

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

if (NOT DEFINED IO_OPENSSL_FOUND)
    message(FATAL_ERROR "IO CMake sctipts internal error, IO_OPENSSL_FOUND is not defined.")
endif()

if (NOT IO_OPENSSL_FOUND)
    list(FILTER TESTS_LIST EXCLUDE REGEX Tls.*)
    list(FILTER TESTS_LIST EXCLUDE REGEX Dtls.*)
    list(FILTER TESTS_LIST EXCLUDE REGEX VersionTest\.cpp)
endif()

set(RUN_TEST_ENV) # TODO: currently this is unused but may be utilized with sanitizers
set(TESTS_EXE_NAME tests)

add_executable(${TESTS_EXE_NAME} ${TESTS_LIST})
target_link_libraries(${TESTS_EXE_NAME}
    io
    GTest::gtest
    Boost::system
    Boost::filesystem
)

if (MSVC)
    target_compile_definitions(${TESTS_EXE_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# TODO: remove????
#target_include_directories(${TESTS_EXE_NAME} PUBLIC
#    "${CMAKE_CURRENT_SOURCE_DIR}"
#    "${CMAKE_SOURCE_DIR}/source"
#    $<BUILD_INTERFACE:$<TARGET_PROPERTY:Boost::headers,INTERFACE_INCLUDE_DIRECTORIES>>
#    ${GTEST_INCLUDE_DIRS}
#)

add_dependencies(${TESTS_EXE_NAME} io)

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

	# Copy openssl dlls to the tests folder
	if (MSVC AND IO_OPENSSL_FOUND)
		# TODO: openssl debug build support (if available) LIB_EAY_DEBUG, SSL_EAY_DEBUG ...
		# TODO: static build of openssl support here (need to skip copying)
		if (NOT EXISTS "${LIB_EAY_RELEASE}")
			message(WARNING "LIB_EAY_RELEASE path is not found")
		endif()
	
		if (NOT EXISTS "${SSL_EAY_RELEASE}")
			message(WARNING "SSL_EAY_RELEASE path is not found")
		endif()

		message("LIB_EAY_RELEASE: ${LIB_EAY_RELEASE}")
		message("SSL_EAY_RELEASE: ${SSL_EAY_RELEASE}")

		get_filename_component(LIB_EAY_NAME "${LIB_EAY_RELEASE}" NAME_WE CACHE)
		message("LIB_EAY_NAME: ${LIB_EAY_NAME}")

		get_filename_component(SSL_EAY_NAME "${SSL_EAY_RELEASE}" NAME_WE CACHE)
		message("SSL_EAY_NAME: ${SSL_EAY_NAME}")

		set(LIB_EAY_DLL_NAME "${LIB_EAY_NAME}.dll")
		set(SSL_EAY_DLL_NAME "${SSL_EAY_NAME}.dll")
		message("LIB_EAY_DLL_NAME: ${LIB_EAY_DLL_NAME}")
		message("SSL_EAY_DLL_NAME: ${SSL_EAY_DLL_NAME}")

		get_filename_component(SSL_LIB_DIR "${LIB_EAY_RELEASE}" DIRECTORY CACHE)
		message("SSL_LIB_DIR: ${SSL_LIB_DIR}")
		get_filename_component(SSL_DLL_DIR "${SSL_LIB_DIR}/../bin" ABSOLUTE CACHE)
		message("SSL_DLL_DIR: ${SSL_DLL_DIR}")

		message("FULL_PATH: ${SSL_DLL_DIR}/${LIB_EAY_DLL_NAME}")

		add_custom_command(TARGET ${TESTS_EXE_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
					${SSL_DLL_DIR}/${LIB_EAY_DLL_NAME}
					${CMAKE_BINARY_DIR}/tests/$<CONFIG>
			COMMAND ${CMAKE_COMMAND} -E copy
					${SSL_DLL_DIR}/${SSL_EAY_DLL_NAME}
					${CMAKE_BINARY_DIR}/tests/$<CONFIG>
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

# TODO: replace hardcoded 'tests' with ${TESTS_EXE_NAME}
if(WIN32) # AND BUILD_SHARED_LIBS
    add_custom_command(TARGET ${TESTS_EXE_NAME} POST_BUILD # Adds a post-build event to tests
#      COMMAND ${CMAKE_COMMAND} -E copy_if_different
#          $<TARGET_PROPERTY:GTest::gtest,IMPORTED_LOCATION_$<UPPER_CASE:$<CONFIG>>>
#          $<TARGET_FILE_DIR:tests>
#          )
	 COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>/../bin/gtest$<$<CONFIG:DEBUG>:d>.dll
          $<TARGET_FILE_DIR:tests>
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

#print_target_properties(GTest::gtest)

# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

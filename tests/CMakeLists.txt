message("-- Searching for GTest")
find_package(GTest REQUIRED CONFIG HINTS ${GTEST_ROOT})
message("-- Found GTest target config in ${GTest_DIR}.")

# Note: tests are ordered from basic ones to advanced ones to allow check them from
# top to bottom if multiple tests fail.
FILE(GLOB TESTS_LIST *.h)
list(APPEND TESTS_LIST
    Main.cpp
    UTCommon.cpp
    CommonTest.cpp
    UserDataHolderTest.cpp
    LoggerTest.cpp
    DisposableTest.cpp
    ScopeExitGuardTest.cpp
    PathTest.cpp
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

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${GTEST_INCLUDE_DIRS}
)

# Added to prevent warning
# ld: warning: direct access in XXX to global weak symbol YYY means the weak symbol cannot be overridden at runtime. This was likely caused by different translation units being compiled with different visibility settings.
# TODO: this needed only if linking with static libs
#if(APPLE)
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
#endif()

set(RUN_TEST_ENV) # TODO: currently this is unused but may be utilized with sanitizers
set(TESTS_EXE_NAME tests)

add_executable(${TESTS_EXE_NAME} ${TESTS_LIST})
target_include_directories(${TESTS_EXE_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/source")
target_link_libraries(${TESTS_EXE_NAME}
    io
    GTest::gtest
    Boost::system
    Boost::filesystem
)

if (WIN32)
    target_compile_definitions(${TESTS_EXE_NAME} PUBLIC IO_TEST_BUILD_FOR_WINDOWS)
endif()

add_dependencies(${TESTS_EXE_NAME} io)

add_custom_command(TARGET ${TESTS_EXE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/data
            ${CMAKE_BINARY_DIR}/tests/$<$<OR:$<PLATFORM_ID:Windows>,$<PLATFORM_ID:Darwin>>:$<CONFIG>>
)

#add_custom_target(printFooKeys
#  COMMAND ${CMAKE_COMMAND} -E echo $<CONFIG>
#)

# TODO: replace hardcoded 'tests' with ${TESTS_EXE_NAME}
if(WIN32) # AND BUILD_SHARED_LIBS
    add_custom_command(TARGET ${TESTS_EXE_NAME} POST_BUILD # Adds a post-build event to tests
      #COMMAND ${CMAKE_COMMAND} -E copy_if_different      # which executes "cmake - E copy_if_different..."
      #    "${CMAKE_BINARY_DIR}/source/$<CONFIG>/io.dll"  # <--this is in-file
      #    $<TARGET_FILE_DIR:tests>                       # <--this is out-file path
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_PROPERTY:GTest::gtest,IMPORTED_LOCATION_$<UPPER_CASE:$<CONFIG>>>
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

find_package(GTest REQUIRED)

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
    EventLoopTest.cpp
    TimerTest.cpp
    FileTest.cpp
    DirTest.cpp
    UdpClientServerTest.cpp
    TcpClientServerTest.cpp
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
    GTest::GTest
    Boost::system
    Boost::filesystem
)

#file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tmp)

add_custom_target(RunTests
    DEPENDS ${TESTS_LIST}
    COMMAND ${CMAKE_COMMAND} -E env ${RUN_TEST_ENV} ./${TESTS_EXE_NAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_dependencies(RunTests ${TESTS_EXE_NAME})

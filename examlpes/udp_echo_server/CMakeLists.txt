cmake_minimum_required(VERSION 3.5.0)

project(udp_echo_server VERSION 1.0.0 LANGUAGES C CXX)

add_executable(main main.cpp)

find_package(tarm-io REQUIRED)
target_link_libraries(main tarm-io::tarm-io)

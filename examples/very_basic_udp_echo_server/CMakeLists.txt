cmake_minimum_required(VERSION 3.5.0)

project(very_basic_udp_echo_server VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
add_executable(very_basic_udp_echo_server main.cpp)

find_package(tarm-io REQUIRED)
target_link_libraries(very_basic_udp_echo_server tarm-io::tarm-io)

#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

# Important notice: if you change this code, ensure that docs are displaying it correctly,
#                   because lines positions are important.

# This example intentionally does not use common CMake code for illustrative purposes

cmake_minimum_required(VERSION 3.5.0)
project(hello_event_loop VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
add_executable(hello_event_loop main.cpp)

find_package(tarm-io REQUIRED)
target_link_libraries(hello_event_loop tarm-io::tarm-io)

if (MSVC) # Copy DLLs if required
    add_custom_command(TARGET hello_event_loop POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_PROPERTY:tarm-io::tarm-io,IMPORTED_LOCATION_$<UPPER_CASE:$<CONFIG>>>"
            "$<TARGET_FILE_DIR:hello_event_loop>")

    if (NOT TARM_IO_SSL_EAY_DLL_PATH STREQUAL "")
        add_custom_command(TARGET hello_event_loop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${TARM_IO_SSL_EAY_DLL_PATH}"
                "$<TARGET_FILE_DIR:hello_event_loop>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${TARM_IO_LIB_EAY_DLL_PATH}"
                "$<TARGET_FILE_DIR:hello_event_loop>"
        )
    endif()
endif()
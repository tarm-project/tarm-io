cmake_minimum_required(VERSION 3.5.0)
project(examples)

# Macro accepts variable number of arguments.
# All arguments except PROJECT_NAME should be source file names.
macro(setup_project_for_examples PROJECT_NAME)
    cmake_minimum_required(VERSION 3.5.0)
    project(${PROJECT_NAME} VERSION 1.0.0 LANGUAGES C CXX)

    set(CMAKE_CXX_STANDARD 11)
    set(SOURCES_LIST "${ARGN}")
    add_executable(${PROJECT_NAME} ${SOURCES_LIST})

    find_package(tarm-io REQUIRED)
    target_link_libraries(${PROJECT_NAME} tarm-io::tarm-io)

    if (MSVC)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_PROPERTY:tarm-io::tarm-io,IMPORTED_LOCATION_$<UPPER_CASE:$<CONFIG>>>"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")

        if (NOT TARM_IO_SSL_EAY_DLL_PATH STREQUAL "")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                    "${TARM_IO_SSL_EAY_DLL_PATH}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
                COMMAND ${CMAKE_COMMAND} -E copy
                    "${TARM_IO_LIB_EAY_DLL_PATH}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            )
        endif()
    endif()
endmacro()

add_subdirectory(hello_event_loop)
add_subdirectory(very_basic_udp_echo_server)
add_subdirectory(udp_echo_server)
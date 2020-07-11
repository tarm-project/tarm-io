#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

# This file adds GTest:: imported targets to current scope

unset(GTest_DIR CACHE)

if (TARM_IO_USE_EXTERNAL_GTEST)
    find_package(GTest REQUIRED CONFIG HINTS ${GTEST_ROOT})
else()
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
                "${PROJECT_SOURCE_DIR}/thirdparty/googletest-${TARM_IO_GTEST_VERSION}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gtest_build")
        if(result)
            message(FATAL_ERROR "CMake step for googletest failed: ${output} \nresult: ${result}")
        endif()

        foreach(config_type Debug;Release)
            message("${config_type}")

            execute_process(COMMAND ${CMAKE_COMMAND} --build . ${TARM_IO_BUILD_JOBS_FLAGS} --config "${config_type}"
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
                "${PROJECT_SOURCE_DIR}/thirdparty/googletest-${TARM_IO_GTEST_VERSION}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gtest_build")
        if(result)
            message(FATAL_ERROR "CMake step for googletest failed: ${output} \nresult: ${result}")
        endif()

        execute_process(COMMAND ${CMAKE_COMMAND} --build . ${TARM_IO_BUILD_JOBS_FLAGS}
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
endif()

message(STATUS "Found GTest ${GTest_VERSION} target config in ${GTest_DIR}.")
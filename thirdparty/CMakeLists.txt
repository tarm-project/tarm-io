#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

# Solution from here https://gitlab.kitware.com/cmake/cmake/issues/17645
if (CMAKE_CONFIGURATION_TYPES)
	set(TARM_IO_PROPAGATE_BUILD_TYPE --config $<CONFIG>)
else()
	set(TARM_IO_PROPAGATE_BUILD_TYPE -DCMAKE_BUILD_TYPE=$<CONFIG>)
endif()

if(WIN32)
  list(APPEND LIBUV_LIB_DEPS
       advapi32
       iphlpapi
       psapi
       shell32
       user32
       userenv
       ws2_32)
else()
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
        # Android has pthread as part of its c library, not as a separate
        # libpthread.so.
        list(APPEND LIBUV_LIB_DEPS pthread)
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "AIX")
    list(APPEND LIBUV_LIB_DEPS perfstat)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    list(APPEND uv_libs dl)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "DragonFly|FreeBSD|NetBSD|OpenBSD")
    list(APPEND LIBUV_LIB_DEPS kvm)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND LIBUV_LIB_DEPS dl rt)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    list(APPEND LIBUV_LIB_DEPS kstat nsl sendfile socket)
endif()

set(LIBUV_ADDITIONAL_FLAGS "")
if (DEFINED IO_SANITIZER)
    set(LIBUV_ADDITIONAL_FLAGS "${LIBUV_ADDITIONAL_FLAGS} -fsanitize=${IO_SANITIZER}")
endif()

# Promoting LIBUV_LIB_DEPS to parent scope
set(LIBUV_LIB_DEPS "${LIBUV_LIB_DEPS}" PARENT_SCOPE)

if (TARM_IO_USE_EXTERNAL_LIBUV)
    return()
endif()

if (NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR "lib")
endif()

set(LIBUV_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/libuv")
set(LIBUV_STATIC_LIB_PATH "${LIBUV_PREFIX}/install/${CMAKE_INSTALL_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}uv_a${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(LIBUV_DYNAMIC_LIB_PATH "${LIBUV_PREFIX}/install/${CMAKE_INSTALL_LIBDIR}/${CMAKE_SHARED_LIBRARY_PREFIX}uv${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(LIBUV_INCLUDE_PATH "${LIBUV_PREFIX}/install/include")

# Workaround for error about non-existent directory in INTERFACE_INCLUDE_DIRECTORIES during CMake configuration step
# https://gitlab.kitware.com/cmake/cmake/-/issues/15052
file(MAKE_DIRECTORY "${LIBUV_INCLUDE_PATH}")

ExternalProject_Add(libuv-bundled
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libuv-${TARM_IO_LIBUV_VERSION}"
    PREFIX "${LIBUV_PREFIX}"
    BINARY_DIR "${LIBUV_PREFIX}/build"
    INSTALL_DIR "${LIBUV_PREFIX}/install"
    CMAKE_ARGS -Dlibuv_buildtests=OFF
               -DCMAKE_INSTALL_PREFIX=${LIBUV_PREFIX}/install
               -DCMAKE_POSITION_INDEPENDENT_CODE=ON
               -DCMAKE_C_FLAGS=${LIBUV_ADDITIONAL_FLAGS}
               -DCMAKE_SHARED_LINKER_FLAGS=${LIBUV_ADDITIONAL_FLAGS}
               -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=${CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE}
               ${TARM_IO_PROPAGATE_BUILD_TYPE}
    BUILD_BYPRODUCTS ${LIBUV_STATIC_LIB_PATH}
    BUILD_BYPRODUCTS ${LIBUV_DYNAMIC_LIB_PATH}
    BUILD_BYPRODUCTS ${LIBUV_INCLUDE_PATH}
)

# IMPORTED target is required for Ninja generator because it needs all rule to be defined at configuration of CMake
if (NOT DEFINED IO_SANITIZER)
    add_library(LibUV::LibUV STATIC IMPORTED GLOBAL)
    set_target_properties(LibUV::LibUV PROPERTIES
        IMPORTED_LOCATION "${LIBUV_STATIC_LIB_PATH}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBUV_INCLUDE_PATH}")
else()
    add_library(LibUV::LibUV SHARED IMPORTED GLOBAL)
    set_target_properties(LibUV::LibUV PROPERTIES
        IMPORTED_LOCATION "${LIBUV_DYNAMIC_LIB_PATH}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBUV_INCLUDE_PATH}")
endif()

add_dependencies(LibUV::LibUV libuv-bundled)







#set(GTEST_ADDITIONAL_FLAGS "")
#
## ==================================================================================================
#
#set(GTEST_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/gtest")
#set(GTEST_STATIC_LIB_PATH "${GTEST_PREFIX}/install/${CMAKE_INSTALL_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX}")
#set(GTEST_DYNAMIC_LIB_PATH "${GTEST_PREFIX}/install/${CMAKE_INSTALL_LIBDIR}/${CMAKE_SHARED_LIBRARY_PREFIX}gtest${CMAKE_SHARED_LIBRARY_SUFFIX}")
#set(GTEST_INCLUDE_PATH "${GTEST_PREFIX}/install/include")
#
#
#ExternalProject_Add(gtest-bundled
#    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/googletest-${TARM_IO_GTEST_VERSION}"
#    PREFIX "${GTEST_PREFIX}"
#    BINARY_DIR "${GTEST_PREFIX}/build"
#    INSTALL_DIR "${GTEST_PREFIX}/install"
#    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX="${GTEST_PREFIX}/install"
#               -DCMAKE_POSITION_INDEPENDENT_CODE=ON
#               -DCMAKE_C_FLAGS=${GTEST_ADDITIONAL_FLAGS}
#               -DCMAKE_CXX_FLAGS=${GTEST_ADDITIONAL_FLAGS}
#               -DCMAKE_SHARED_LINKER_FLAGS=${GTEST_ADDITIONAL_FLAGS}
#               -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=${CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE}
#               ${TARM_IO_PROPAGATE_BUILD_TYPE}
#    BUILD_BYPRODUCTS ${GTEST_STATIC_LIB_PATH}
#    BUILD_BYPRODUCTS ${GTEST_DYNAMIC_LIB_PATH}
#    BUILD_BYPRODUCTS ${GTEST_INCLUDE_PATH}
#)
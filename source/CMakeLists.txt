#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

include(TestBigEndian)
TEST_BIG_ENDIAN(IO_IS_BIG_ENDIAN)

# Openssl
if (NOT BUILD_SHARED_LIBS)
    set(OPENSSL_USE_STATIC_LIBS ON)
endif()

set(IO_OPENSSL_FOUND FALSE PARENT_SCOPE)
message(STATUS "Searching for OpenSSL...")
find_package(OpenSSL)

if (OpenSSL_FOUND)
    # DOC: warn of danger of having openssl installed in /usr/local/include
    # https://langui.sh/2015/07/24/osx-clang-include-lib-search-paths/
    message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
    # Yes, need to set this twice. Once for current scope and once for parent scope.
    set(IO_OPENSSL_FOUND TRUE PARENT_SCOPE)
    set(IO_OPENSSL_FOUND TRUE)
endif()

# Files
FILE(GLOB IO_HEADERS_LIST
        io/*.h
        io/global/*.h
        io/detail/*.h
        io/path_impl/*.h)

list(APPEND IO_SOURCE_LIST
        ${IO_HEADERS_LIST}
        io/detail/Common.cpp
        io/detail/OpenSslInitHelper.cpp
        io/detail/PeerId.cpp
        io/global/Configuration.cpp
        io/global/Version.cpp
        io/path_impl/CodecvtErrorCategory.cpp
        io/path_impl/PathTraits.cpp
        io/path_impl/HashRange.cpp
        io/path_impl/Utf8CodecvtFacet.cpp
        io/path_impl/WindowsFileCodecvt.cpp
        io/ByteSwap.cpp
        io/Convert.cpp
        io/Dir.cpp
        io/DtlsClient.cpp
        io/DtlsConnectedClient.cpp
        io/DtlsServer.cpp
        io/Endpoint.cpp
        io/Error.cpp
        io/EventLoop.cpp
        io/File.cpp
        io/Logger.cpp
        io/Path.cpp
        io/RefCounted.cpp
        io/Removable.cpp
        io/StatData.cpp
        io/StatusCode.cpp
        io/Timer.cpp
        io/TcpClient.cpp
        io/TcpConnectedClient.cpp
        io/TcpServer.cpp
        io/TlsTcpClient.cpp
        io/TlsTcpConnectedClient.cpp
        io/TlsTcpServer.cpp
        io/UdpClient.cpp
        io/UdpPeer.cpp
        io/UdpServer.cpp
        io/UserDataHolder.cpp
)

set(IO_SOURCE_EXCLUDE_PATTERN "=") # '=' is just a placeholder value which will definately will not be used for paths.
if (NOT IO_OPENSSL_FOUND)
    set(IO_SOURCE_EXCLUDE_PATTERN Tls.*|Dtls.*|OpenSsl.*|Version\..*)
    list(FILTER IO_SOURCE_LIST EXCLUDE REGEX ${IO_SOURCE_EXCLUDE_PATTERN})
endif()

if (${CMAKE_VERSION} VERSION_LESS "3.8.2")
    source_group(tarm-io FILES ${IO_SOURCE_LIST})
else()
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${IO_SOURCE_LIST})
endif()

add_library(tarm-io ${IO_SOURCE_LIST})
set_target_properties(tarm-io PROPERTIES DEBUG_POSTFIX _d)
target_link_libraries(tarm-io PUBLIC ${LIBUV_LIB_DEPS})

# DOC: there are problems with CMake to find different OpenSSL versions.
# https://gitlab.kitware.com/cmake/cmake/-/issues/18352
# One part could be found from one version and second from another
if (IO_OPENSSL_FOUND)
    target_link_libraries(tarm-io PRIVATE OpenSSL::SSL) # WAS PUBLIC
    target_link_libraries(tarm-io PRIVATE OpenSSL::Crypto) # WAS PUBLIC
    target_compile_definitions(tarm-io PRIVATE IO_HAS_OPENSSL)

    if (OPENSSL_ROOT_DIR)
        target_include_directories(tarm-io PUBLIC ${OPENSSL_ROOT_DIR}/include)
    endif()
endif()

if (IO_USE_EXTERNAL_LIBUV)
    target_link_libraries(tarm-io PUBLIC LibUV::LibUV)
else()
    if (DEFINED IO_SANITIZER)
        target_link_libraries(tarm-io PRIVATE ${LIBUV_DYNAMIC_LIB_PATH})
    else()
        target_link_libraries(tarm-io PRIVATE ${LIBUV_STATIC_LIB_PATH})
    endif()

    target_include_directories(tarm-io PUBLIC $<BUILD_INTERFACE:${LIBUV_INCLUDE_PATH}>)

    add_dependencies(tarm-io libuv)
endif()

target_include_directories(tarm-io PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_include_directories(tarm-io SYSTEM INTERFACE "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_definitions(tarm-io PRIVATE IO_BUILDING_DLL)

if (TARM_IO_PLATFORM_LINUX)
    target_compile_options(tarm-io PUBLIC -pthread)
    set_target_properties(tarm-io PROPERTIES LINK_FLAGS -pthread)
endif()

if (MSVC)
	# https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-3-c4996?view=vs-2019
	target_compile_definitions(tarm-io PRIVATE _SCL_SECURE_NO_WARNINGS)
endif()

if (IO_IS_BIG_ENDIAN)
    target_compile_definitions(tarm-io PRIVATE IO_BIG_ENDIAN)
endif()

if (WIN32)
    target_compile_definitions(tarm-io PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(tarm-io PRIVATE NOGDI)
endif()

if (TARM_IO_PLATFORM_LINUX)
    target_compile_definitions(tarm-io PUBLIC TARM_IO_PLATFORM_LINUX)
elseif (TARM_IO_PLATFORM_WINDOWS)
    target_compile_definitions(tarm-io PUBLIC TARM_IO_PLATFORM_WINDOWS)
elseif (TARM_IO_PLATFORM_MACOSX)
    target_compile_definitions(tarm-io PUBLIC TARM_IO_PLATFORM_MACOSX)
else()
    message(FATAL_ERROR "Can not determine platform of libtarm-io build.")
endif()

# TODO: no exceptions even in regular builds????
if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
#    target_compile_options(tarm-io PRIVATE -fno-exceptions)
endif()

if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET tarm-io POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/tests/$<CONFIG>/"
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${CMAKE_BINARY_DIR}/source/$<CONFIG>/tarm-io.dll"
              "${CMAKE_BINARY_DIR}/tests/$<CONFIG>/"
              )
endif()

# installation
# Unfortunately can not use target property PUBLIC_HEADER here
# because of https://gitlab.kitware.com/cmake/cmake/issues/16739
install(DIRECTORY io DESTINATION include/tarm
   FILES_MATCHING PATTERN *.h
   PATTERN detail EXCLUDE
   REGEX "${IO_SOURCE_EXCLUDE_PATTERN}" EXCLUDE)

install(TARGETS tarm-io EXPORT tarm-ioTargets
    LIBRARY
        DESTINATION lib
        COMPONENT Libraries
    ARCHIVE
        DESTINATION lib
        COMPONENT Libraries
)

set(cmake_package_name tarm-io)
set(targets_export_name ${cmake_package_name}Targets CACHE INTERNAL "")
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated" CACHE INTERNAL "")
set(cmake_files_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${cmake_package_name}")
set(version_file "${generated_dir}/${cmake_package_name}ConfigVersion.cmake")
write_basic_package_version_file(${version_file} COMPATIBILITY AnyNewerVersion)
install(EXPORT ${targets_export_name}
    NAMESPACE ${cmake_package_name}::
    DESTINATION ${cmake_files_install_dir})
set(config_file "${generated_dir}/${cmake_package_name}Config.cmake")
configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/tarm-ioConfig.cmake.in"
    "${config_file}" INSTALL_DESTINATION ${cmake_files_install_dir})
install(FILES ${version_file} ${config_file}
    DESTINATION ${cmake_files_install_dir})

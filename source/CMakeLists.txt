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
# TODO: look like this is not required, need to check some older systems
#if (LINUX)
#    find_package(PkgConfig REQUIRED)
#    pkg_check_modules(SSL libssl)
#    pkg_check_modules(CRYPTO libcrypto)
#
#    if (SSL_FOUND AND CRYPTO_FOUND)
#        set(IO_OPENSSL_FOUND TRUE)
#        target_include_directories(io PUBLIC $<BUILD_INTERFACE:${SSL_INCLUDEDIR}>)
#        target_include_directories(io PUBLIC $<BUILD_INTERFACE:${CRYPTO_INCLUDEDIR}>)
#        target_link_libraries(io PUBLIC ${SSL_LIBRARIES})
#        target_link_libraries(io PUBLIC ${CRYPTO_LIBRARIES})
#    endif()
#else()
    message(STATUS "Searching for OpenSSL...")
    find_package(OpenSSL)

    if (OpenSSL_FOUND)
        set(IO_OPENSSL_FOUND TRUE PARENT_SCOPE)
        set(IO_OPENSSL_FOUND TRUE)
    endif()
#endif()

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
    source_group(io FILES ${IO_SOURCE_LIST})
else()
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${IO_SOURCE_LIST})
endif()

add_library(io ${IO_SOURCE_LIST})
target_link_libraries(io PUBLIC ${LIBUV_LIB_DEPS})

if (IO_OPENSSL_FOUND)
    target_link_libraries(io PUBLIC OpenSSL::SSL)
    target_link_libraries(io PUBLIC OpenSSL::Crypto)
    target_compile_definitions(io PRIVATE IO_HAS_OPENSSL)
endif()

if (IO_USE_EXTERNAL_LIBUV)
    target_link_libraries(io PUBLIC LibUV::LibUV)
else()
    if (DEFINED IO_SANITIZER)
        target_link_libraries(io PRIVATE ${LIBUV_DYNAMIC_LIB_PATH})
    else()
        target_link_libraries(io PRIVATE ${LIBUV_STATIC_LIB_PATH})
    endif()

    target_include_directories(io PUBLIC $<BUILD_INTERFACE:${LIBUV_INCLUDE_PATH}>)

    add_dependencies(io libuv)
endif()

target_include_directories(io PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_include_directories(io SYSTEM INTERFACE "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_definitions(io PRIVATE IO_BUILDING_DLL)

if (IO_IS_BIG_ENDIAN)
    target_compile_definitions(io PRIVATE IO_BIG_ENDIAN)
endif()

if (WIN32)
    target_compile_definitions(io PUBLIC IO_BUILD_FOR_WINDOWS)
    target_compile_definitions(io PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(io PRIVATE NOGDI)
endif()

# TODO: no exceptions even in regular builds????
if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
#    target_compile_options(io PRIVATE -fno-exceptions)
endif()

# installation
# Unfortunately can not use target property PUBLIC_HEADER here
# because of https://gitlab.kitware.com/cmake/cmake/issues/16739
install(DIRECTORY io DESTINATION include
   FILES_MATCHING PATTERN *.h
   PATTERN detail EXCLUDE
   REGEX "${IO_SOURCE_EXCLUDE_PATTERN}" EXCLUDE)

install(TARGETS io EXPORT ioTargets
    LIBRARY
        DESTINATION lib
        COMPONENT Libraries
#        NAMELINK_COMPONENT Development #TODO: fixme??? Or remove???
    ARCHIVE
        DESTINATION lib
        COMPONENT Libraries
#   PUBLIC_HEADER
#        DESTINATION include
#        COMPONENT Development
)

set(cmake_package_name io)
set(targets_export_name ${cmake_package_name}Targets CACHE INTERNAL "")
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated" CACHE INTERNAL "")
set(cmake_files_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${cmake_package_name}")
set(version_file "${generated_dir}/${cmake_package_name}ConfigVersion.cmake")
write_basic_package_version_file(${version_file} COMPATIBILITY AnyNewerVersion)
install(EXPORT ${targets_export_name}
  NAMESPACE ${cmake_package_name}::
  DESTINATION ${cmake_files_install_dir})
set(config_file "${generated_dir}/${cmake_package_name}Config.cmake")
configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/ioConfig.cmake.in"
  "${config_file}" INSTALL_DESTINATION ${cmake_files_install_dir})
install(FILES ${version_file} ${config_file}
  DESTINATION ${cmake_files_install_dir})

if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET io POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${CMAKE_BINARY_DIR}/source/$<CONFIG>/io.dll"
              "${CMAKE_BINARY_DIR}/tests/$<CONFIG>/"
              )
endif()

#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

include(TestBigEndian)
TEST_BIG_ENDIAN(TARM_IO_IS_BIG_ENDIAN)

# Openssl
if (NOT BUILD_SHARED_LIBS)
    set(OPENSSL_USE_STATIC_LIBS ON)
endif()

set(TARM_IO_OPENSSL_FOUND FALSE PARENT_SCOPE)
message(STATUS "Searching for OpenSSL...")
find_package(OpenSSL)

function(tarm_io_msvc_arch_name_from_lib PATH_TO_LIB_FILE RESULT_ARCH_NAME)
    get_filename_component(TARM_IO_MSVC_LIB_TOOL_DIR "${CMAKE_LINKER}" DIRECTORY CACHE)

    set(DUMPBIN_EXE_TOOL_OUTPUT)
    execute_process(COMMAND "${TARM_IO_MSVC_LIB_TOOL_DIR}/dumpbin.exe" "/headers" "/NOLOGO" "${PATH_TO_LIB_FILE}"
                    OUTPUT_VARIABLE DUMPBIN_EXE_TOOL_OUTPUT)
    string(REGEX MATCH "machine \\(x[0-9]+\\)" _NOT_USED "${DUMPBIN_EXE_TOOL_OUTPUT}")
    string(REGEX REPLACE "machine \\((x[0-9]+)\\)"
       "\\1" ARCH_NAME
       "${CMAKE_MATCH_0}")

    set(${RESULT_ARCH_NAME} "${ARCH_NAME}" PARENT_SCOPE)
endfunction()

if (OpenSSL_FOUND)
    # DOC: warn of danger of having openssl installed in /usr/local/include
    # https://langui.sh/2015/07/24/osx-clang-include-lib-search-paths/
    message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")

    if (OPENSSL_ROOT_DIR)
        set(TARM_IO_OPENSSL_ROOT_DIR "${OPENSSL_ROOT_DIR}")
    else()
        get_filename_component(TARM_IO_OPENSSL_ROOT_DIR "${OPENSSL_INCLUDE_DIR}/.." ABSOLUTE)
    endif()
    message("TARM_IO_OPENSSL_ROOT_DIR: ${TARM_IO_OPENSSL_ROOT_DIR}")

    if (MSVC)
        # TODO: openssl debug build support (if available) LIB_EAY_DEBUG, SSL_EAY_DEBUG ...
        if (NOT EXISTS "${LIB_EAY_RELEASE}")
            message(FATAL_ERROR "LIB_EAY_RELEASE path is not found")
        endif()

        if (NOT EXISTS "${SSL_EAY_RELEASE}")
            message(FATAL_ERROR "SSL_EAY_RELEASE path is not found")
        endif()

        message("LIB_EAY_RELEASE: ${LIB_EAY_RELEASE}")
        message("SSL_EAY_RELEASE: ${SSL_EAY_RELEASE}")

        # Examining only one lib because it is unlikely that they both have different architecture
        tarm_io_msvc_arch_name_from_lib("${LIB_EAY_RELEASE}" LIB_EAY_ARCHITECTURE)

        # TODO: need to generalize and extract this code
        if (DEFINED CMAKE_GENERATOR_PLATFORM)
            if (CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
                set(TARM_IO_TARGET_ARCHITECTURE "x86")
            else()
                set(TARM_IO_TARGET_ARCHITECTURE "${CMAKE_GENERATOR_PLATFORM}")
            endif()
        else()
            set(TARM_IO_TARGET_ARCHITECTURE "x86") # default for MSVC
        endif()

        message("CMAKE_GENERATOR_PLATFORM: ${CMAKE_GENERATOR_PLATFORM}")
        message("TARM_IO_TARGET_ARCHITECTURE: ${TARM_IO_TARGET_ARCHITECTURE}")
        message("LIB_EAY_ARCHITECTURE: ${LIB_EAY_ARCHITECTURE}")
        if (NOT TARM_IO_TARGET_ARCHITECTURE STREQUAL ${LIB_EAY_ARCHITECTURE})
            message("CMake has found OpenSSL in '${TARM_IO_OPENSSL_ROOT_DIR}' but its architecture '${LIB_EAY_ARCHITECTURE}' "
                    "is different from this build one '${TARM_IO_TARGET_ARCHITECTURE}', "
                    "so marking OpenSSL as not found")
        else()
            # Yes, need to set this twice. Once for current scope and once for parent scope.
            set(TARM_IO_OPENSSL_FOUND TRUE PARENT_SCOPE)
            set(TARM_IO_OPENSSL_FOUND TRUE)
        endif()
    else()
        # Yes, need to set this twice. Once for current scope and once for parent scope.
        set(TARM_IO_OPENSSL_FOUND TRUE PARENT_SCOPE)
        set(TARM_IO_OPENSSL_FOUND TRUE)
    endif()
endif()

# Files
FILE(GLOB TARM_IO_HEADERS_LIST
        io/*.h
        io/core/*.h
        io/detail/*.h
        io/fs/*.h
        io/fs/detail/*.h
        io/global/*.h
        io/net/*.h
        io/net/detail/*.h
        io/path_impl/*.h)

list(APPEND TARM_IO_SOURCE_LIST
        ${TARM_IO_HEADERS_LIST}
        io/core/VariableLengthSize.cpp
        io/detail/Common.cpp
        io/detail/LibuvCompatibility.cpp
        io/global/Configuration.cpp
        io/fs/Dir.cpp
        io/fs/File.cpp
        io/fs/Functions.cpp
        io/fs/Path.cpp
        io/fs/StatData.cpp
        io/fs/path_impl/CodecvtErrorCategory.cpp
        io/fs/path_impl/PathTraits.cpp
        io/fs/path_impl/HashRange.cpp
        io/fs/path_impl/Utf8CodecvtFacet.cpp
        io/fs/path_impl/WindowsFileCodecvt.cpp
        io/net/detail/OpenSslInitHelper.cpp
        io/net/detail/PeerId.cpp
        io/net/Dns.cpp
        io/net/DtlsClient.cpp
        io/net/DtlsConnectedClient.cpp
        io/net/DtlsServer.cpp
        io/net/Endpoint.cpp
        io/net/ProtocolVersion.cpp
        io/net/TcpClient.cpp
        io/net/TcpConnectedClient.cpp
        io/net/TcpServer.cpp
        io/net/TlsClient.cpp
        io/net/TlsConnectedClient.cpp
        io/net/TlsServer.cpp
        io/net/UdpClient.cpp
        io/net/UdpPeer.cpp
        io/net/UdpServer.cpp
        io/ByteSwap.cpp
        io/Convert.cpp
        io/Error.cpp
        io/EventLoop.cpp
        io/Logger.cpp
        io/RefCounted.cpp
        io/Removable.cpp
        io/StatusCode.cpp
        io/Timer.cpp
        io/UserDataHolder.cpp
        io/Version.cpp
)

set(TARM_IO_SOURCE_EXCLUDE_PATTERN "=") # '=' is just a placeholder value which will definitely will not be used for paths.
if (NOT TARM_IO_OPENSSL_FOUND)
    set(TARM_IO_SOURCE_EXCLUDE_PATTERN Tls.*|Dtls.*|OpenSsl.*|Version\..*)
    list(FILTER TARM_IO_SOURCE_LIST EXCLUDE REGEX ${TARM_IO_SOURCE_EXCLUDE_PATTERN})
endif()

if (${CMAKE_VERSION} VERSION_LESS "3.8.2")
    source_group(tarm-io FILES ${TARM_IO_SOURCE_LIST})
else()
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${TARM_IO_SOURCE_LIST})
endif()

add_library(tarm-io ${TARM_IO_SOURCE_LIST})
set_target_properties(tarm-io PROPERTIES DEBUG_POSTFIX _d)
target_link_libraries(tarm-io PUBLIC ${LIBUV_LIB_DEPS})

# Last parameter is return value
function(tarm_io_msvc_dll_name_from_lib PATH_TO_LIB_FILE RESULT_DLL_NAME)
    get_filename_component(TARM_IO_MSVC_LIB_TOOL_DIR "${CMAKE_LINKER}" DIRECTORY CACHE)

    set(LIB_EXE_TOOL_OUTPUT)
    execute_process(COMMAND "${TARM_IO_MSVC_LIB_TOOL_DIR}/lib.exe" "/list" "/NOLOGO" "${PATH_TO_LIB_FILE}"
                    OUTPUT_VARIABLE LIB_EXE_TOOL_OUTPUT)

    string(FIND "${LIB_EXE_TOOL_OUTPUT}" ".dll" DLL_NAME_STR_POS)
    #if (DLL_NAME_STR_POS == -1) # TODO:
    string(SUBSTRING "${LIB_EXE_TOOL_OUTPUT}" 0 ${DLL_NAME_STR_POS} DLL_NAME)
    set(${RESULT_DLL_NAME} "${DLL_NAME}" PARENT_SCOPE)
endfunction()

function(tarm_io_msvc_define_openssl_dll_paths SSL_EAY_DLL_PATH LIB_EAY_DLL_PATH)
    tarm_io_msvc_dll_name_from_lib("${LIB_EAY_RELEASE}" LIB_EAY_DLL_NAME)
    tarm_io_msvc_dll_name_from_lib("${SSL_EAY_RELEASE}" SSL_EAY_DLL_NAME)
    message("LIB_EAY_DLL_NAME: ${LIB_EAY_DLL_NAME}")
    message("SSL_EAY_DLL_NAME: ${SSL_EAY_DLL_NAME}")

    # TODO: introduce function here????? (2 block are very similar)
    find_file(ROOT_DIR_LIB_EAY_DLL_PATH
             ${LIB_EAY_DLL_NAME}.dll
             PATHS "${TARM_IO_OPENSSL_ROOT_DIR}"
             PATH_SUFFIXES bin
             NO_DEFAULT_PATH)
    if (ROOT_DIR_LIB_EAY_DLL_PATH STREQUAL "ROOT_DIR_LIB_EAY_DLL_PATH-NOTFOUND")
        message("${LIB_EAY_DLL_NAME}.dll not found in specified folders, searching system-wide...")
        find_file(SYS_DIR_LIB_EAY_DLL_PATH
                  ${LIB_EAY_DLL_NAME}.dll
                  REQUIRED)
        message("System LIB_EAY: ${SYS_DIR_LIB_EAY_DLL_PATH}")
        set(TMP_LIB_EAY_DLL_PATH "")
    else()
        set(TMP_LIB_EAY_DLL_PATH "${ROOT_DIR_LIB_EAY_DLL_PATH}")
    endif()

    find_file(ROOT_DIR_SSL_EAY_DLL_PATH
              ${SSL_EAY_DLL_NAME}.dll
              PATHS "${TARM_IO_OPENSSL_ROOT_DIR}"
              PATH_SUFFIXES bin
              NO_DEFAULT_PATH)
    if (ROOT_DIR_SSL_EAY_DLL_PATH STREQUAL "ROOT_DIR_SSL_EAY_DLL_PATH-NOTFOUND")
        message("${SSL_EAY_DLL_NAME}.dll not found in specified folders, searching system-wide...")
        find_file(SYS_DIR_SSL_EAY_DLL_PATH
                  ${SSL_EAY_DLL_NAME}.dll
                  REQUIRED)
        message("System SSL_EAY: ${SYS_DIR_SSL_EAY_DLL_PATH}")
        set(TMP_SSL_EAY_DLL_PATH "")
    else()
        set(TMP_SSL_EAY_DLL_PATH "${ROOT_DIR_SSL_EAY_DLL_PATH}")
    endif()

    set(${LIB_EAY_DLL_PATH} "${TMP_LIB_EAY_DLL_PATH}" PARENT_SCOPE)
    set(${SSL_EAY_DLL_PATH} "${TMP_SSL_EAY_DLL_PATH}" PARENT_SCOPE)
endfunction()

# DOC: there are problems with CMake to find different OpenSSL versions.
# https://gitlab.kitware.com/cmake/cmake/-/issues/18352
# One part could be found from one version and second from another
if (TARM_IO_OPENSSL_FOUND)
    target_link_libraries(tarm-io PRIVATE OpenSSL::SSL) # WAS PUBLIC
    target_link_libraries(tarm-io PRIVATE OpenSSL::Crypto) # WAS PUBLIC
    target_compile_definitions(tarm-io PRIVATE TARM_IO_HAS_OPENSSL)

    target_include_directories(tarm-io PUBLIC "${OPENSSL_INCLUDE_DIR}")

    # Search for DLLs if any available
    if (MSVC)
        set(TARM_IO_LIB_EAY_DLL_PATH)
        set(TARM_IO_SSL_EAY_DLL_PATH)
        tarm_io_msvc_define_openssl_dll_paths(TARM_IO_LIB_EAY_DLL_PATH TARM_IO_SSL_EAY_DLL_PATH)
        set(TARM_IO_LIB_EAY_DLL_PATH "${TARM_IO_LIB_EAY_DLL_PATH}" PARENT_SCOPE)
        set(TARM_IO_SSL_EAY_DLL_PATH "${TARM_IO_SSL_EAY_DLL_PATH}" PARENT_SCOPE)
        message("TARM_IO_LIB_EAY_DLL_PATH: ${TARM_IO_LIB_EAY_DLL_PATH}")
        message("TARM_IO_SSL_EAY_DLL_PATH: ${TARM_IO_SSL_EAY_DLL_PATH}")
    endif()
endif()

if (NOT TARM_IO_USE_EXTERNAL_LIBUV)
    # Using bundled version which we need to build
    add_dependencies(tarm-io LibUV::LibUV)
endif()
target_link_libraries(tarm-io PRIVATE LibUV::LibUV)

target_include_directories(tarm-io PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/io>)
target_include_directories(tarm-io SYSTEM INTERFACE "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>")
target_include_directories(tarm-io SYSTEM INTERFACE "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>/tarm/io")
target_compile_definitions(tarm-io PRIVATE TARM_IO_BUILDING_DLL)

if (TARM_IO_PLATFORM_LINUX)
    target_compile_options(tarm-io PUBLIC -pthread)
    set_target_properties(tarm-io PROPERTIES LINK_FLAGS -pthread)
endif()

if (MSVC)
    # https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-3-c4996?view=vs-2019
    target_compile_definitions(tarm-io PRIVATE _SCL_SECURE_NO_WARNINGS)
endif()

if (TARM_IO_IS_BIG_ENDIAN)
    target_compile_definitions(tarm-io PRIVATE TARM_IO_BIG_ENDIAN)
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

# Disable exceptions (experimental, need additional research, but main motivation is performance)
if (TARM_IO_PLATFORM_LINUX OR
    TARM_IO_PLATFORM_MACOSX OR
    CMAKE_BUILD_TYPE STREQUAL "Coverage")

    if (NOT DEFINED TARM_IO_SANITIZER)
        target_compile_options(tarm-io PRIVATE -fno-exceptions)
    endif()
elseif (TARM_IO_PLATFORM_WINDOWS)
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

	# Disabling warning caused by disabled exceptions
	# C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.16.27023\include\xlocale(319):
	# warning C4530: C++ exception handler used, but unwind semantics are not enabled.
	# Specify /EHsc [D:\tarm-io\build\source\tarm-io.vcxproj]
    target_compile_options(tarm-io PRIVATE "/wd4530")
endif()

if (MSVC)
	# No warnings like
	# warning C4996: 'getenv': This function or variable may be unsafe.
	# Consider using _dupenv_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details.
	target_compile_definitions(tarm-io PRIVATE "_CRT_SECURE_NO_WARNINGS")
endif()


if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET tarm-io POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/tests/$<CONFIG>/"
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${CMAKE_BINARY_DIR}/source/$<CONFIG>/tarm-io$<$<CONFIG:DEBUG>:$<TARGET_PROPERTY:tarm-io,DEBUG_POSTFIX>>.dll"
              "${CMAKE_BINARY_DIR}/tests/$<CONFIG>/"
              )
endif()

# installation
# Unfortunately can not use target property PUBLIC_HEADER here
# because of https://gitlab.kitware.com/cmake/cmake/issues/16739
install(DIRECTORY io DESTINATION include/tarm
   FILES_MATCHING PATTERN *.h
   PATTERN detail EXCLUDE
   REGEX "${TARM_IO_SOURCE_EXCLUDE_PATTERN}" EXCLUDE)

install(TARGETS tarm-io EXPORT tarm-ioTargets
    RUNTIME
        DESTINATION bin
        COMPONENT Libraries
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
    NAMESPACE TARM::
    DESTINATION ${cmake_files_install_dir})
set(config_file "${generated_dir}/${cmake_package_name}Config.cmake")
configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/tarm-ioConfig.cmake.in"
    "${config_file}" INSTALL_DESTINATION ${cmake_files_install_dir})
install(FILES ${version_file} ${config_file}
    DESTINATION ${cmake_files_install_dir})


#add_custom_target(debug_generator_expressions
#    COMMAND ${CMAKE_COMMAND} -E echo $<$<CONFIG:DEBUG>:$<TARGET_PROPERTY:tarm-io,DEBUG_POSTFIX>>
#)

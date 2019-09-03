FILE(GLOB IO_HEADERS_LIST
        io/*.h
        io/global/*.h
        io/detail/*.h)

list(APPEND IO_SOURCE_LIST
        ${IO_HEADERS_LIST}
        io/global/Configuration.cpp
        io/Common.cpp
        io/EventLoop.cpp
        io/Disposable.cpp
        io/Dir.cpp
        io/File.cpp
        io/Logger.cpp
        io/StatData.cpp
        io/Status.cpp
        io/StatusCode.cpp
        io/Timer.cpp
        io/TcpClient.cpp
        io/TcpConnectedClient.cpp
        io/TcpServer.cpp
        io/UdpClient.cpp
        io/UdpClient.h
        io/UdpServer.h
        io/UdpServer.cpp
        io/UserDataHolder.cpp
)

if (${CMAKE_VERSION} VERSION_LESS "3.8.2")
    source_group(io FILES ${IO_SOURCE_LIST})
else()
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${IO_SOURCE_LIST})
endif()

add_library(io SHARED ${IO_SOURCE_LIST})

if (DEFINED IO_SANITIZER)
    target_link_libraries(io
                          PRIVATE ${LIBUV_DYNAMIC_LIB_PATH}
                          PUBLIC ${LIBUV_LIB_DEPS})
else()
    target_link_libraries(io
                          PRIVATE ${LIBUV_STATIC_LIB_PATH}
                          PUBLIC ${LIBUV_LIB_DEPS})
endif()

target_include_directories(io PUBLIC $<BUILD_INTERFACE:$<TARGET_PROPERTY:Boost::headers,INTERFACE_INCLUDE_DIRECTORIES>>)
target_include_directories(io PUBLIC $<BUILD_INTERFACE:${LIBUV_INCLUDE_PATH}>)
target_include_directories(io PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_include_directories(io SYSTEM INTERFACE "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_definitions(io PRIVATE IO_BUILDING_DLL)

# TODO: no exceptions even in regular builds????
if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
    target_compile_options(io PRIVATE -fno-exceptions)
endif()

add_dependencies(io libuv)

# installation
# Unfortunately can not use target property PUBLIC_HEADER here
# because of https://gitlab.kitware.com/cmake/cmake/issues/16739
install(DIRECTORY io DESTINATION include
   FILES_MATCHING PATTERN *.h
   PATTERN detail EXCLUDE)

install(TARGETS io EXPORT ioTargets
    LIBRARY
        DESTINATION lib
        COMPONENT Libraries
#        NAMELINK_COMPONENT Development #TODO: fixme??? Or remove???
#    PUBLIC_HEADER
#        DESTINATION include
#        COMPONENT Development
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
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

if(WIN32) # TODO: AND BUILD_SHARED_LIBS
    add_custom_command(TARGET io POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${CMAKE_BINARY_DIR}/source/$<CONFIG>/io.dll"
              "${CMAKE_BINARY_DIR}/tests/$<CONFIG>/"
              )
endif()

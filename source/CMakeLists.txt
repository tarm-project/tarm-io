# TODO: need to find better name than io
# TODO: looks like need to move io-related CMake code into io folder
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
#include_directories("source")

if (${CMAKE_VERSION} VERSION_LESS "3.8.2")
    source_group(io FILES ${IO_SOURCE_LIST})
else()
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${IO_SOURCE_LIST})
endif()

add_library(io SHARED ${IO_SOURCE_LIST})
set_target_properties(io PROPERTIES PUBLIC_HEADER "${IO_HEADERS_LIST}")
if (DEFINED IO_SANITIZER)
    target_link_libraries(io ${LIBUV_DYNAMIC_LIB_PATH})
else()
    target_link_libraries(io ${LIBUV_STATIC_LIB_PATH})
endif()
target_include_directories(io PUBLIC $<TARGET_PROPERTY:Boost::headers,INTERFACE_INCLUDE_DIRECTORIES>)
add_dependencies(io libuv)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
#include_directories(${LIBUV_INCLUDE_DIR})

install(TARGETS io
    LIBRARY
        DESTINATION lib
        COMPONENT Libraries
#        NAMELINK_COMPONENT Development TODO: fixme???
    PUBLIC_HEADER
        DESTINATION include
        COMPONENT Development)
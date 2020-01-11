add_executable(client Client.cpp)
target_link_libraries(client io)

add_executable(server Server.cpp)
target_link_libraries(server io)


#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

if (IO_OPENSSL_FOUND)
    add_executable(client Client.cpp)
    target_link_libraries(client tarm-io)

    add_executable(server Server.cpp)
    target_link_libraries(server tarm-io)
endif()
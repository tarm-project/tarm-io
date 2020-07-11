#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

# This file adds Boost:: imported targets to current scope

unset(boost_filesystem_DIR CACHE)
unset(boost_system_DIR CACHE)
unset(boost_headers_DIR CACHE)

if (WIN32)
   set(Boost_USE_STATIC_LIBS ON) # TODO: need to make this optional
endif()
#set(Boost_DEBUG ON)
find_package(Boost COMPONENTS system filesystem REQUIRED)

# Boost build withoud imported CMake targets (creating them manually)
if(NOT TARGET Boost::headers)
    add_library(Boost::headers IMPORTED INTERFACE GLOBAL)
    set_property(TARGET Boost::headers PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
endif()

if(NOT TARGET Boost::system)
    add_library(Boost::system IMPORTED INTERFACE GLOBAL)
    set_property(TARGET Boost::system PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
    set_property(TARGET Boost::system PROPERTY INTERFACE_LINK_LIBRARIES ${Boost_SYSTEM_LIBRARY})
endif()

if(NOT TARGET Boost::filesystem)
    add_library(Boost::filesystem IMPORTED INTERFACE GLOBAL)
    set_property(TARGET Boost::filesystem PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
    set_property(TARGET Boost::filesystem PROPERTY INTERFACE_LINK_LIBRARIES ${Boost_FILESYSTEM_LIBRARY})
endif()

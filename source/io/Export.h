/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

// Based on code from here https://gcc.gnu.org/wiki/Visibility

#if defined _WIN32 || defined __CYGWIN__
    #ifdef IO_BUILDING_DLL
        #ifdef __GNUC__
            #define IO_DLL_PUBLIC __attribute__ ((dllexport))
        #else
            #define IO_DLL_PUBLIC __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
        #endif
    #else
        #ifdef __GNUC__
            #define IO_DLL_PUBLIC __attribute__ ((dllimport))
        #else
            #define IO_DLL_PUBLIC __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
        #endif
    #endif

    #define IO_DLL_PRIVATE
    #define IO_DLL_PUBLIC_CLASS_UNIX_ONLY
#else
    #define IO_DLL_PUBLIC __attribute__ ((visibility ("default")))
    #define IO_DLL_PRIVATE  __attribute__ ((visibility ("hidden")))

    // This is a workaround to problem of export typeinfo for classes on Unix.
    // Unfortunately if some member of class is exported, typeinfo is not.
    // And there is no easy way to do this explicitly.
    // But on Windows if we export entire class it is exported with all data members
    // which is not desirable and produce warnings. So this intermediate solution was introduced.
    #define IO_DLL_PUBLIC_CLASS_UNIX_ONLY IO_DLL_PUBLIC
#endif

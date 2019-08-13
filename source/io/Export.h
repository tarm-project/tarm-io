#pragma once

// Code from here https://gcc.gnu.org/wiki/Visibility

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
#else
    #if __GNUC__ >= 4
        #define IO_DLL_PUBLIC __attribute__ ((visibility ("default")))
        #define IO_DLL_PRIVATE  __attribute__ ((visibility ("hidden")))
    #else
        #define IO_DLL_PUBLIC
        #define IO_DLL_PRIVATE
    #endif
#endif
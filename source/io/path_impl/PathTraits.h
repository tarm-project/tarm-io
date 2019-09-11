//  Copyright Beman Dawes 2009
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt
//  Library home page: http://www.boost.org/libs/filesystem

#pragma once

#include "io/Export.h"

#include <cwchar>  // for mbstate_t
#include <string>
#include <vector>
#include <list>
#include <iterator>
#include <locale>
#include <type_traits>
#include <system_error>
#include <assert.h>

namespace io {

IO_DLL_PUBLIC const std::error_category& codecvt_error_category();
//  uses std::codecvt_base::result used for error codes:
//
//    ok:       Conversion successful.
//    partial:  Not all source characters converted; one or more additional source
//              characters are needed to produce the final target character, or the
//              size of the target intermediate buffer was too small to hold the result.
//    error:    A character in the source could not be converted to the target encoding.
//    noconv:   The source and target characters have the same type and encoding, so no
//              conversion was necessary.

namespace path_traits {

using codecvt_type =  std::codecvt<wchar_t, char, std::mbstate_t> ;

//  is_pathable type trait; allows disabling over-agressive class path member templates

template <class T>
struct is_pathable { static const bool value = false; };

template<> struct is_pathable<char*>                  { static const bool value = true; };
template<> struct is_pathable<const char*>            { static const bool value = true; };
template<> struct is_pathable<wchar_t*>               { static const bool value = true; };
template<> struct is_pathable<const wchar_t*>         { static const bool value = true; };
template<> struct is_pathable<std::string>            { static const bool value = true; };
template<> struct is_pathable<std::wstring>           { static const bool value = true; };
template<> struct is_pathable<std::vector<char> >     { static const bool value = true; };
template<> struct is_pathable<std::vector<wchar_t> >  { static const bool value = true; };
template<> struct is_pathable<std::list<char> >       { static const bool value = true; };
template<> struct is_pathable<std::list<wchar_t> >    { static const bool value = true; };

//  Pathable empty
template <class Container>
inline typename std::enable_if<!std::is_array<Container>::value, bool>::type
empty(const Container& c) {
    return c.begin() == c.end();
}

template <class T>
inline bool empty(T* const& c_str) {
    assert(c_str);
    return !*c_str;
}

template <typename T, size_t N>
inline bool empty(T (&x)[N]) {
    return !x[0];
}

// value types differ  ---------------------------------------------------------------//
//
//   A from_end argument of 0 is less efficient than a known end, so use only if needed

//  with codecvt

// from_end is 0 for null terminated MBCS
IO_DLL_PUBLIC
void convert(const char* from, const char* from_end, std::wstring& to, const codecvt_type& cvt);

// from_end is 0 for null terminated MBCS
IO_DLL_PUBLIC
void convert(const wchar_t* from, const wchar_t* from_end, std::string& to, const codecvt_type& cvt);

IO_DLL_PUBLIC
void convert(const char* from, std::wstring& to, const codecvt_type& cvt);

IO_DLL_PUBLIC
void convert(const wchar_t* from, std::string& to, const codecvt_type& cvt);

//  without codecvt

// from_end is 0 for null terminated MBCS
IO_DLL_PUBLIC
void convert(const char* from, const char* from_end, std::wstring& to);

// from_end is 0 for null terminated MBCS
IO_DLL_PUBLIC
void convert(const wchar_t* from, const wchar_t* from_end, std::string& to);

IO_DLL_PUBLIC
void convert(const char* from, std::wstring& to);

IO_DLL_PUBLIC
void convert(const wchar_t* from, std::string& to);

// value types same  -----------------------------------------------------------------//

// char with codecvt
IO_DLL_PUBLIC
void convert(const char* from, const char* from_end, std::string& to, const codecvt_type&);

IO_DLL_PUBLIC
void convert(const char* from, std::string& to, const codecvt_type&);

// wchar_t with codecvt

IO_DLL_PUBLIC
void convert(const wchar_t* from, const wchar_t* from_end, std::wstring& to, const codecvt_type&);

IO_DLL_PUBLIC
void convert(const wchar_t* from, std::wstring& to, const codecvt_type&);

// char without codecvt

IO_DLL_PUBLIC
void convert(const char* from, const char* from_end, std::string& to);

IO_DLL_PUBLIC
void convert(const char* from, std::string& to);

// wchar_t without codecvt

IO_DLL_PUBLIC
void convert(const wchar_t* from, const wchar_t* from_end, std::wstring& to);

IO_DLL_PUBLIC
void convert(const wchar_t* from, std::wstring& to);

//  Source dispatch  -----------------------------------------------------------------//

//  contiguous containers with codecvt
template <class U>
inline void dispatch(const std::string& c, U& to, const codecvt_type& cvt) {
    if (c.size())
        convert(&*c.begin(), &*c.begin() + c.size(), to, cvt);
}

template <class U> inline
void dispatch(const std::wstring& c, U& to, const codecvt_type& cvt) {
    if (c.size())
        convert(&*c.begin(), &*c.begin() + c.size(), to, cvt);
}

template <class U> inline
void dispatch(const std::vector<char>& c, U& to, const codecvt_type& cvt) {
    if (c.size())
        convert(&*c.begin(), &*c.begin() + c.size(), to, cvt);
}

template <class U> inline
void dispatch(const std::vector<wchar_t>& c, U& to, const codecvt_type& cvt) {
    if (c.size())
        convert(&*c.begin(), &*c.begin() + c.size(), to, cvt);
}

//  contiguous containers without codecvt
template <class U> inline
void dispatch(const std::string& c, U& to) {
    if (c.size())
        convert(&*c.begin(), &*c.begin() + c.size(), to);
}

template <class U> inline
void dispatch(const std::wstring& c, U& to) {
    if (c.size())
        convert(&*c.begin(), &*c.begin() + c.size(), to);
}

template <class U> inline
void dispatch(const std::vector<char>& c, U& to) {
    if (c.size())
        convert(&*c.begin(), &*c.begin() + c.size(), to);
}

template <class U> inline
void dispatch(const std::vector<wchar_t>& c, U& to) {
    if (c.size())
        convert(&*c.begin(), &*c.begin() + c.size(), to);
}

//  non-contiguous containers with codecvt
template <class Container, class U>
inline typename std::enable_if<!std::is_array<Container>::value, void>::type
dispatch(const Container& c, U& to, const codecvt_type& cvt) {
    if (c.size()) {
        std::basic_string<typename Container::value_type> s(c.begin(), c.end());
        convert(s.c_str(), s.c_str()+s.size(), to, cvt);
    }
}

//  c_str
template <class T, class U>
inline void dispatch(T* const& c_str, U& to, const codecvt_type& cvt) {
    assert(c_str);
    convert(c_str, to, cvt);
}

//  Note: there is no dispatch on C-style arrays because the array may
//  contain a string smaller than the array size.

//  non-contiguous containers without codecvt
template <class Container, class U> inline
typename std::enable_if<!std::is_array<Container>::value, void>::type
dispatch(const Container& c, U& to) {
    if (c.size()) {
        std::basic_string<typename Container::value_type> seq(c.begin(), c.end());
        convert(seq.c_str(), seq.c_str()+seq.size(), to);
    }
}

//  c_str
template <class T, class U> inline
void dispatch(T* const& c_str, U& to) {
    assert(c_str);
    convert(c_str, to);
}

} // namespace path_traits
} // namespace io

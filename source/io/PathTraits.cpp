//  Copyright Beman Dawes 2008, 2009
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt
//  Library home page: http://www.boost.org/libs/filesystem

#include "PathTraits.h"

#include "Path.h"   // for path::codecvt()

// TODO: boost dependency here
#include <boost/scoped_array.hpp>

#include <locale>   // for codecvt_base::result
#include <cstring>  // for strlen
#include <cwchar>   // for wcslen

namespace pt = io::path_traits;

//--------------------------------------------------------------------------------------//
//                                  configuration                                       //
//--------------------------------------------------------------------------------------//

#ifndef IO_FILESYSTEM_CODECVT_BUF_SIZE
# define IO_FILESYSTEM_CODECVT_BUF_SIZE 256
#endif

namespace {

const std::size_t default_codecvt_buf_size = IO_FILESYSTEM_CODECVT_BUF_SIZE;


//--------------------------------------------------------------------------------------//
//                                                                                      //
//  The public convert() functions do buffer management, and then forward to the        //
//  convert_impl() functions for the actual call to the codecvt facet.                   //
//                                                                                      //
//--------------------------------------------------------------------------------------//

//--------------------------------------------------------------------------------------//
//                      convert_impl const char* to wstring                             //
//--------------------------------------------------------------------------------------//

  void convert_impl(const char* from,
                    const char* from_end,
                    wchar_t* to, wchar_t* to_end,
                    std::wstring & target,
                    const pt::codecvt_type & cvt) {
    std::mbstate_t state  = std::mbstate_t();  // perhaps unneeded, but cuts bug reports
    const char* from_next;
    wchar_t* to_next;

    std::codecvt_base::result res;

    if ((res=cvt.in(state, from, from_end, from_next, to, to_end, to_next)) != std::codecvt_base::ok) {
        // TODO: we do not want to throw exceptions
        throw std::system_error(res, io::codecvt_error_category(), "io::Path codecvt to wstring");
    }
    target.append(to, to_next);
}

//--------------------------------------------------------------------------------------//
//                      convert_impl const wchar_t* to string                           //
//--------------------------------------------------------------------------------------//

  void convert_impl(const wchar_t* from,
                    const wchar_t* from_end,
                    char* to, char* to_end,
                    std::string & target,
                    const pt::codecvt_type & cvt) {
    std::mbstate_t state  = std::mbstate_t();  // perhaps unneeded, but cuts bug reports
    const wchar_t* from_next;
    char* to_next;

    std::codecvt_base::result res;

    if ((res=cvt.out(state, from, from_end, from_next, to, to_end, to_next)) != std::codecvt_base::ok) {
        // TODO: we do not want to throw exceptions
        throw std::system_error(res, io::codecvt_error_category(), "io::Path codecvt to string");
    }
    target.append(to, to_next);
}

}  // namespace

//--------------------------------------------------------------------------------------//
//                                   path_traits                                        //
//--------------------------------------------------------------------------------------//

namespace io {
namespace path_traits {

//--------------------------------------------------------------------------------------//
//                          convert const char* to wstring                              //
//--------------------------------------------------------------------------------------//

  IO_DLL_PUBLIC
  void convert(const char* from,
                const char* from_end,    // 0 for null terminated MBCS
                std::wstring & to,
                const codecvt_type & cvt)
  {
    assert(from);

    if (!from_end) { // null terminated
        from_end = from + std::strlen(from);
    }

    if (from == from_end)
        return;

    std::size_t buf_size = (from_end - from) * 3;  // perhaps too large, but that's OK

    //  dynamically allocate a buffer only if source is unusually large
    if (buf_size > default_codecvt_buf_size) {
        boost::scoped_array< wchar_t > buf(new wchar_t [buf_size]);
        convert_impl(from, from_end, buf.get(), buf.get()+buf_size, to, cvt);
    } else {
        wchar_t buf[default_codecvt_buf_size];
        convert_impl(from, from_end, buf, buf+default_codecvt_buf_size, to, cvt);
    }
}

//--------------------------------------------------------------------------------------//
//                         convert const wchar_t* to string                            //
//--------------------------------------------------------------------------------------//

  IO_DLL_PUBLIC
  void convert(const wchar_t* from,
                const wchar_t* from_end,  // 0 for null terminated MBCS
                std::string & to,
                const codecvt_type & cvt) {
    assert(from);

    if (!from_end) { // null terminated
        from_end = from + std::wcslen(from);
    }

    if (from == from_end)
        return;

    //  The codecvt length functions may not be implemented, and I don't really
    //  understand them either. Thus this code is just a guess; if it turns
    //  out the buffer is too small then an error will be reported and the code
    //  will have to be fixed.
    std::size_t buf_size = (from_end - from) * 4;  // perhaps too large, but that's OK
    buf_size += 4;  // encodings like shift-JIS need some prefix space

    //  dynamically allocate a buffer only if source is unusually large
    if (buf_size > default_codecvt_buf_size) {
        boost::scoped_array< char > buf(new char [buf_size]);
        convert_impl(from, from_end, buf.get(), buf.get()+buf_size, to, cvt);
    } else {
        char buf[default_codecvt_buf_size];
        convert_impl(from, from_end, buf, buf+default_codecvt_buf_size, to, cvt);
    }
}

void convert(const char* from, std::wstring& to, const codecvt_type& cvt) {
    assert(from);
    convert(from, 0, to, cvt);
}


void convert(const wchar_t* from, std::string& to, const codecvt_type& cvt) {
    assert(from);
    convert(from, 0, to, cvt);
}

void convert(const char* from, const char* from_end, std::string& to, const codecvt_type&) {
    assert(from);
    assert(from_end);
    to.append(from, from_end);
}

void convert(const char* from, std::string& to, const codecvt_type&) {
    assert(from);
    to += from;
}

void convert(const wchar_t* from, const wchar_t* from_end, std::wstring& to, const codecvt_type&) {
    assert(from);
    assert(from_end);
    to.append(from, from_end);
}

void convert(const wchar_t* from, std::wstring& to, const codecvt_type&) {
    assert(from);
    to += from;
}

void convert(const char* from, const char* from_end, std::string& to) {
    assert(from);
    assert(from_end);
    to.append(from, from_end);
}

void convert(const char* from, std::string& to) {
    assert(from);
    to += from;
}

void convert(const wchar_t* from, const wchar_t* from_end, std::wstring& to) {
    assert(from);
    assert(from_end);
    to.append(from, from_end);
}

void convert(const wchar_t* from, std::wstring& to) {
    assert(from);
    to += from;
}

void convert(const char* from, const char* from_end, std::wstring& to) {
    convert(from, from_end, to, path::codecvt());
}

void convert(const wchar_t* from, const wchar_t* from_end, std::string& to) {
    convert(from, from_end, to, path::codecvt());
}

void convert(const char* from, std::wstring& to) {
    assert(!!from);
    convert(from, 0, to, path::codecvt());
}

void convert(const wchar_t* from, std::string & to) {
    assert(!!from);
    convert(from, 0, to, path::codecvt());
}

} // namespace path_traits
} // namespace io

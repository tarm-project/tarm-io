//  Copyright Beman Dawes 2002-2005, 2009
//  Copyright Vladimir Prus 2002
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt
//  Library home page: http://www.boost.org/libs/filesystem

#pragma once

#include "Export.h"
#include "path_impl/HashRange.h"
#include "path_impl/PathTraits.h"
#include "path_impl/QuotedManip.h"

#include <string>
#include <iterator>
#include <cstring>
#include <iosfwd>
#include <stdexcept>
#include <cassert>
#include <locale>
#include <algorithm>
#include <type_traits>
#include <cstdint>

namespace io {
namespace path_detail { // intentionally don't use io::detail to not bring internal filesystem functions into ADL via path_constants

template< typename Char, Char Separator, Char PreferredSeparator, Char Dot >
struct PathConstants {
    using path_constants_base = PathConstants<Char, Separator, PreferredSeparator, Dot>;
    using value_type = Char;

    static constexpr value_type separator = Separator;
    static constexpr value_type preferred_separator = PreferredSeparator;
    static constexpr value_type dot = Dot;
};

template<typename Char, Char Separator, Char PreferredSeparator, Char Dot >
constexpr typename PathConstants< Char, Separator, PreferredSeparator, Dot >::value_type
PathConstants< Char, Separator, PreferredSeparator, Dot >::separator;

template< typename Char, Char Separator, Char PreferredSeparator, Char Dot >
constexpr typename PathConstants< Char, Separator, PreferredSeparator, Dot >::value_type
PathConstants< Char, Separator, PreferredSeparator, Dot >::preferred_separator;

template< typename Char, Char Separator, Char PreferredSeparator, Char Dot >
constexpr typename PathConstants< Char, Separator, PreferredSeparator, Dot >::value_type
PathConstants< Char, Separator, PreferredSeparator, Dot >::dot;

} // namespace path_detail

  //------------------------------------------------------------------------------------//
  //                                                                                    //
  //                                    class path                                      //
  //                                                                                    //
  //------------------------------------------------------------------------------------//

  class Path : public path_detail::PathConstants<
#ifdef BOOST_WINDOWS_API
      wchar_t, L'/', L'\\', L'.'
#else
      char, '/', '/', '.'
#endif
    >
  {
  public:

    //  value_type is the character type used by the operating system API to
    //  represent paths.

    typedef path_constants_base::value_type value_type;
    typedef std::basic_string<value_type>  string_type;
    typedef std::codecvt<wchar_t, char,
                         std::mbstate_t>   codecvt_type;


    //  ----- character encoding conversions -----

    //  Following the principle of least astonishment, path input arguments
    //  passed to or obtained from the operating system via objects of
    //  class path behave as if they were directly passed to or
    //  obtained from the O/S API, unless conversion is explicitly requested.
    //
    //  POSIX specfies that path strings are passed unchanged to and from the
    //  API. Note that this is different from the POSIX command line utilities,
    //  which convert according to a locale.
    //
    //  Thus for POSIX, char strings do not undergo conversion.  wchar_t strings
    //  are converted to/from char using the path locale or, if a conversion
    //  argument is given, using a conversion object modeled on
    //  std::wstring_convert.
    //
    //  The path locale, which is global to the thread, can be changed by the
    //  imbue() function. It is initialized to an implementation defined locale.
    //
    //  For Windows, wchar_t strings do not undergo conversion. char strings
    //  are converted using the "ANSI" or "OEM" code pages, as determined by
    //  the AreFileApisANSI() function, or, if a conversion argument is given,
    //  using a conversion object modeled on std::wstring_convert.
    //
    //  See m_pathname comments for further important rationale.

    //  TODO: rules needed for operating systems that use / or .
    //  differently, or format directory paths differently from file paths.
    //
    //  **********************************************************************************
    //
    //  More work needed: How to handle an operating system that may have
    //  slash characters or dot characters in valid filenames, either because
    //  it doesn't follow the POSIX standard, or because it allows MBCS
    //  filename encodings that may contain slash or dot characters. For
    //  example, ISO/IEC 2022 (JIS) encoding which allows switching to
    //  JIS x0208-1983 encoding. A valid filename in this set of encodings is
    //  0x1B 0x24 0x42 [switch to X0208-1983] 0x24 0x2F [U+304F Kiragana letter KU]
    //                                             ^^^^
    //  Note that 0x2F is the ASCII slash character
    //
    //  **********************************************************************************

    //  Supported source arguments: half-open iterator range, container, c-array,
    //  and single pointer to null terminated string.

    //  All source arguments except pointers to null terminated byte strings support
    //  multi-byte character strings which may have embedded nulls. Embedded null
    //  support is required for some Asian languages on Windows.

    //  "const codecvt_type& cvt=codecvt()" default arguments are not used because this
    //  limits the impact of locale("") initialization failures on POSIX systems to programs
    //  that actually depend on locale(""). It further ensures that exceptions thrown
    //  as a result of such failues occur after main() has started, so can be caught.

    //  -----  constructors  -----

    Path() noexcept {}
    Path(const Path& p) : m_pathname(p.m_pathname) {}

    template <class Source>
    Path(Source const& source,
      typename std::enable_if<path_traits::is_pathable<typename std::decay<Source>::type>::value >::type* = 0) {
      path_traits::dispatch(source, m_pathname);
    }

    Path(const value_type* s) : m_pathname(s) {}
    Path(value_type* s) : m_pathname(s) {}
    Path(const string_type& s) : m_pathname(s) {}
    Path(string_type& s) : m_pathname(s) {}

    Path(Path&& p) noexcept : m_pathname(std::move(p.m_pathname)) {}
    Path& operator=(Path&& p) noexcept { m_pathname = std::move(p.m_pathname); return *this; }

    template <class Source>
    Path(Source const& source, const codecvt_type& cvt) {
      path_traits::dispatch(source, m_pathname, cvt);
    }

    template <class InputIterator>
    Path(InputIterator begin, InputIterator end) {
      if (begin != end) {
        // convert requires contiguous string, so copy
        std::basic_string<typename std::iterator_traits<InputIterator>::value_type> seq(begin, end);
        path_traits::convert(seq.c_str(), seq.c_str()+seq.size(), m_pathname);
      }
    }

    template <class InputIterator>
    Path(InputIterator begin, InputIterator end, const codecvt_type& cvt) {
      if (begin != end) {
        // convert requires contiguous string, so copy
        std::basic_string<typename std::iterator_traits<InputIterator>::value_type>
          seq(begin, end);
        path_traits::convert(seq.c_str(), seq.c_str()+seq.size(), m_pathname, cvt);
      }
    }

    //  -----  assignments  -----

    Path& operator=(const Path& p) {
      m_pathname = p.m_pathname;
      return *this;
    }

    template <class Source>
    typename std::enable_if<path_traits::is_pathable<typename std::decay<Source>::type>::value, Path&>::type
    operator=(Source const& source) {
      m_pathname.clear();
      path_traits::dispatch(source, m_pathname);
      return *this;
    }

    //  value_type overloads


    Path& operator=(const value_type* ptr) {m_pathname = ptr; return *this;} // required in case ptr overlaps *this
    Path& operator=(value_type* ptr) {m_pathname = ptr; return *this;} // required in case ptr overlaps *this
    Path& operator=(const string_type& s) {m_pathname = s; return *this;}
    Path& operator=(string_type& s)       {m_pathname = s; return *this;}

    Path& assign(const value_type* ptr, const codecvt_type&) {m_pathname = ptr; return *this;} // required in case ptr overlaps *this

    template <class Source>
    Path& assign(Source const& source, const codecvt_type& cvt) {
      m_pathname.clear();
      path_traits::dispatch(source, m_pathname, cvt);
      return *this;
    }

    template <class InputIterator>
    Path& assign(InputIterator begin, InputIterator end)
    {
      m_pathname.clear();
      if (begin != end)
      {
        std::basic_string<typename std::iterator_traits<InputIterator>::value_type>
          seq(begin, end);
        path_traits::convert(seq.c_str(), seq.c_str()+seq.size(), m_pathname);
      }
      return *this;
    }

    template <class InputIterator>
    Path& assign(InputIterator begin, InputIterator end, const codecvt_type& cvt)
    {
      m_pathname.clear();
      if (begin != end)
      {
        std::basic_string<typename std::iterator_traits<InputIterator>::value_type>
          seq(begin, end);
        path_traits::convert(seq.c_str(), seq.c_str()+seq.size(), m_pathname, cvt);
      }
      return *this;
    }

    //  -----  concatenation  -----

    template <class Source>
    typename std::enable_if<path_traits::is_pathable<typename std::decay<Source>::type>::value, Path&>::type
    operator+=(Source const& source) {
      return concat(source);
    }

    //  value_type overloads. Same rationale as for constructors above
    Path& operator+=(const Path& p)         { m_pathname += p.m_pathname; return *this; }
    Path& operator+=(const value_type* ptr) { m_pathname += ptr; return *this; }
    Path& operator+=(value_type* ptr)       { m_pathname += ptr; return *this; }
    Path& operator+=(const string_type& s)  { m_pathname += s; return *this; }
    Path& operator+=(string_type& s)        { m_pathname += s; return *this; }
    Path& operator+=(value_type c)          { m_pathname += c; return *this; }

    template <class CharT>
    typename std::enable_if<std::is_integral<CharT>::value, Path&>::type
    operator+=(CharT c) {
      CharT tmp[2];
      tmp[0] = c;
      tmp[1] = 0;
      return concat(tmp);
    }

    template <class Source>
    Path& concat(Source const& source) {
      path_traits::dispatch(source, m_pathname);
      return *this;
    }

    template <class Source>
    Path& concat(Source const& source, const codecvt_type& cvt) {
      path_traits::dispatch(source, m_pathname, cvt);
      return *this;
    }

    template <class InputIterator>
    Path& concat(InputIterator begin, InputIterator end) {
      if (begin == end)
        return *this;
      std::basic_string<typename std::iterator_traits<InputIterator>::value_type>
        seq(begin, end);
      path_traits::convert(seq.c_str(), seq.c_str()+seq.size(), m_pathname);
      return *this;
    }

    template <class InputIterator>
    Path& concat(InputIterator begin, InputIterator end, const codecvt_type& cvt) {
      if (begin == end)
        return *this;
      std::basic_string<typename std::iterator_traits<InputIterator>::value_type>
        seq(begin, end);
      path_traits::convert(seq.c_str(), seq.c_str()+seq.size(), m_pathname, cvt);
      return *this;
    }

    //  -----  appends  -----

    //  if a separator is added, it is the preferred separator for the platform;
    //  slash for POSIX, backslash for Windows

    IO_DLL_PUBLIC Path& operator/=(const Path& p);

    template <class Source>
    typename std::enable_if<path_traits::is_pathable<typename std::decay<Source>::type>::value, Path&>::type
    operator/=(Source const& source) {
      return append(source);
    }

    IO_DLL_PUBLIC Path& operator/=(const value_type* ptr);
    Path& operator/=(value_type* ptr)
    {
      return this->operator/=(const_cast<const value_type*>(ptr));
    }
    Path& operator/=(const string_type& s) { return this->operator/=(Path(s)); }
    Path& operator/=(string_type& s)       { return this->operator/=(Path(s)); }

    Path& append(const value_type* ptr)  // required in case ptr overlaps *this
    {
      this->operator/=(ptr);
      return *this;
    }

    Path& append(const value_type* ptr, const codecvt_type&)  // required in case ptr overlaps *this
    {
      this->operator/=(ptr);
      return *this;
    }

    template <class Source>
    Path& append(Source const& source);

    template <class Source>
    Path& append(Source const& source, const codecvt_type& cvt);

    template <class InputIterator>
    Path& append(InputIterator begin, InputIterator end);

    template <class InputIterator>
    Path& append(InputIterator begin, InputIterator end, const codecvt_type& cvt);

    //  -----  modifiers  -----

    void clear() noexcept { m_pathname.clear(); }
#   ifdef BOOST_WINDOWS_API
    IO_DLL_PUBLIC Path& make_preferred();  // change slashes to backslashes
#   else // BOOST_POSIX_API
    Path& make_preferred() { return *this; }  // POSIX no effect
#   endif
    IO_DLL_PUBLIC Path& remove_filename();
    IO_DLL_PUBLIC Path& remove_trailing_separator();
    IO_DLL_PUBLIC Path& replace_extension(const Path& new_extension = Path());
    void swap(Path& rhs) noexcept { m_pathname.swap(rhs.m_pathname); }

    //  -----  observers  -----

    //  For operating systems that format file paths differently than directory
    //  paths, return values from observers are formatted as file names unless there
    //  is a trailing separator, in which case returns are formatted as directory
    //  paths. POSIX and Windows make no such distinction.

    //  Implementations are permitted to return const values or const references.

    //  The string or path returned by an observer are specified as being formatted
    //  as "native" or "generic".
    //
    //  For POSIX, these are all the same format; slashes and backslashes are as input and
    //  are not modified.
    //
    //  For Windows,   native:    as input; slashes and backslashes are not modified;
    //                            this is the format of the internally stored string.
    //                 generic:   backslashes are converted to slashes

    //  -----  native format observers  -----

    const string_type&  native() const noexcept  { return m_pathname; }
    const value_type*   c_str() const noexcept   { return m_pathname.c_str(); }
    string_type::size_type size() const noexcept { return m_pathname.size(); }

    template <class String>
    String string() const;

    template <class String>
    String string(const codecvt_type& cvt) const;

#   ifdef BOOST_WINDOWS_API
    const std::string string() const
    {
      std::string tmp;
      if (!m_pathname.empty())
        path_traits::convert(m_pathname.c_str(), m_pathname.c_str()+m_pathname.size(),
        tmp);
      return tmp;
    }
    const std::string string(const codecvt_type& cvt) const
    {
      std::string tmp;
      if (!m_pathname.empty())
        path_traits::convert(m_pathname.c_str(), m_pathname.c_str()+m_pathname.size(),
          tmp, cvt);
      return tmp;
    }

    //  string_type is std::wstring, so there is no conversion
    const std::wstring& wstring() const { return m_pathname; }
    const std::wstring& wstring(const codecvt_type&) const { return m_pathname; }
#   else   // BOOST_POSIX_API
    //  string_type is std::string, so there is no conversion
    const std::string& string() const { return m_pathname; }
    const std::string& string(const codecvt_type&) const { return m_pathname; }

    const std::wstring wstring() const
    {
      std::wstring tmp;
      if (!m_pathname.empty())
        path_traits::convert(m_pathname.c_str(), m_pathname.c_str()+m_pathname.size(), tmp);
      return tmp;
    }
    const std::wstring wstring(const codecvt_type& cvt) const
    {
      std::wstring tmp;
      if (!m_pathname.empty())
        path_traits::convert(m_pathname.c_str(), m_pathname.c_str()+m_pathname.size(),
          tmp, cvt);
      return tmp;
    }
#   endif

    //  -----  generic format observers  -----

    //  Experimental generic function returning generic formatted path (i.e. separators
    //  are forward slashes). Motivation: simpler than a family of generic_*string
    //  functions.
#   ifdef BOOST_WINDOWS_API
    IO_DLL_PUBLIC Path generic_path() const;
#   else
    Path generic_path() const { return Path(*this); }
#   endif

    template <class String>
    String generic_string() const;

    template <class String>
    String generic_string(const codecvt_type& cvt) const;

#   ifdef BOOST_WINDOWS_API
    const std::string   generic_string() const { return generic_path().string(); }
    const std::string   generic_string(const codecvt_type& cvt) const { return generic_path().string(cvt); }
    const std::wstring  generic_wstring() const { return generic_path().wstring(); }
    const std::wstring  generic_wstring(const codecvt_type&) const { return generic_wstring(); }
#   else // BOOST_POSIX_API
    //  On POSIX-like systems, the generic format is the same as the native format
    const std::string&  generic_string() const  { return m_pathname; }
    const std::string&  generic_string(const codecvt_type&) const  { return m_pathname; }
    const std::wstring  generic_wstring() const { return this->wstring(); }
    const std::wstring  generic_wstring(const codecvt_type& cvt) const { return this->wstring(cvt); }
#   endif

    //  -----  compare  -----

    IO_DLL_PUBLIC int compare(const Path& p) const noexcept;  // generic, lexicographical
    int compare(const std::string& s) const { return compare(Path(s)); }
    int compare(const value_type* s) const  { return compare(Path(s)); }

    //  -----  decomposition  -----

    IO_DLL_PUBLIC Path  root_path() const;
    IO_DLL_PUBLIC Path  root_name() const;         // returns 0 or 1 element path
                                                   // even on POSIX, root_name() is non-empty() for network paths
    IO_DLL_PUBLIC Path  root_directory() const;    // returns 0 or 1 element path
    IO_DLL_PUBLIC Path  relative_path() const;
    IO_DLL_PUBLIC Path  parent_path() const;
    IO_DLL_PUBLIC Path  filename() const;          // returns 0 or 1 element path
    IO_DLL_PUBLIC Path  stem() const;              // returns 0 or 1 element path
    IO_DLL_PUBLIC Path  extension() const;         // returns 0 or 1 element path

    //  -----  query  -----

    bool empty() const noexcept { return m_pathname.empty(); }
    bool filename_is_dot() const;
    bool filename_is_dot_dot() const;
    bool has_root_path() const       { return has_root_directory() || has_root_name(); }
    bool has_root_name() const       { return !root_name().empty(); }
    bool has_root_directory() const  { return !root_directory().empty(); }
    bool has_relative_path() const   { return !relative_path().empty(); }
    bool has_parent_path() const     { return !parent_path().empty(); }
    bool has_filename() const        { return !m_pathname.empty(); }
    bool has_stem() const            { return !stem().empty(); }
    bool has_extension() const       { return !extension().empty(); }
    bool is_relative() const         { return !is_absolute(); }
    bool is_absolute() const
    {
      // Windows CE has no root name (aka drive letters)
#     if defined(BOOST_WINDOWS_API) && !defined(UNDER_CE)
      return has_root_name() && has_root_directory();
#     else
      return has_root_directory();
#     endif
    }

    //  -----  lexical operations  -----

    IO_DLL_PUBLIC Path lexically_normal() const;
    IO_DLL_PUBLIC Path lexically_relative(const Path& base) const;
    Path lexically_proximate(const Path& base) const {
      Path tmp(lexically_relative(base));
      return tmp.empty() ? *this : tmp;
    }

    //  -----  iterators  -----

    class iterator;
    typedef iterator const_iterator;
    class reverse_iterator;
    typedef reverse_iterator const_reverse_iterator;

    IO_DLL_PUBLIC iterator begin() const;
    IO_DLL_PUBLIC iterator end() const;
    reverse_iterator rbegin() const;
    reverse_iterator rend() const;

    //  -----  static member functions  -----

    static IO_DLL_PUBLIC std::locale imbue(const std::locale& loc);
    static IO_DLL_PUBLIC const codecvt_type&  codecvt();

//--------------------------------------------------------------------------------------//
//                            class path private members                                //
//--------------------------------------------------------------------------------------//

  private:

// TODO: revise these pragmas
#   if defined(_MSC_VER)
#     pragma warning(push) // Save warning settings
#     pragma warning(disable : 4251) // disable warning: class 'std::basic_string<_Elem,_Traits,_Ax>'
#   endif                            // needs to have dll-interface...
/*
      m_pathname has the type, encoding, and format required by the native
      operating system. Thus for POSIX and Windows there is no conversion for
      passing m_pathname.c_str() to the O/S API or when obtaining a path from the
      O/S API. POSIX encoding is unspecified other than for dot and slash
      characters; POSIX just treats paths as a sequence of bytes. Windows
      encoding is UCS-2 or UTF-16 depending on the version.
*/
    string_type  m_pathname;  // Windows: as input; backslashes NOT converted to slashes,
                              // slashes NOT converted to backslashes
#   if defined(_MSC_VER)
#     pragma warning(pop) // restore warning settings.
#   endif

    //  Returns: If separator is to be appended, m_pathname.size() before append. Otherwise 0.
    //  Note: An append is never performed if size()==0, so a returned 0 is unambiguous.
    IO_DLL_PUBLIC string_type::size_type m_append_separator_if_needed();

    IO_DLL_PUBLIC void m_erase_redundant_separator(string_type::size_type sep_pos);
    IO_DLL_PUBLIC string_type::size_type m_parent_path_end() const;

    // Was qualified; como433beta8 reports:
    //    warning #427-D: qualified name is not allowed in member declaration
    friend class iterator;
    friend bool operator<(const Path& lhs, const Path& rhs);

    // see Path::iterator::increment/decrement comment below
    static IO_DLL_PUBLIC void m_path_iterator_increment(Path::iterator& it);
    static IO_DLL_PUBLIC void m_path_iterator_decrement(Path::iterator& it);

  };  // class path

  namespace detail
  {
    IO_DLL_PUBLIC
      int lex_compare(Path::iterator first1, Path::iterator last1, Path::iterator first2, Path::iterator last2);
    IO_DLL_PUBLIC
      const Path& dot_path();
    IO_DLL_PUBLIC
      const Path& dot_dot_path();
  } // namespace detail

  //------------------------------------------------------------------------------------//
  //                             class Path::iterator                                   //
  //------------------------------------------------------------------------------------//

class Path::iterator : public std::iterator<
                          std::bidirectional_iterator_tag, // iterator_category
                          Path,                            // value_type
                          long,                            // difference_type
                          const Path*,                     // pointer
                          Path&                            // reference
                        > {
    friend class io::Path;
    friend class io::Path::reverse_iterator;
    friend void m_path_iterator_increment(Path::iterator & it);
    friend void m_path_iterator_decrement(Path::iterator & it);

public:
    const Path& operator*() const { return dereference(); }
    const Path* operator->() const { return &dereference(); }

    iterator& operator++() {
        increment();
        return *this;
    }

    iterator operator++(int) {
        auto tmp = *this;
        increment();
        return tmp;
    }

    iterator& operator--() {
        decrement();
        return *this;
    }

    iterator operator--(int) {
        auto tmp = *this;
        decrement();
        return tmp;
    }

    bool operator==(const iterator& rhs) const { return equal(rhs);}
    bool operator!=(const iterator& rhs) const { return !equal(rhs);}

private:
    const Path& dereference() const { return m_element; }
    bool equal(const iterator& rhs) const { return m_path_ptr == rhs.m_path_ptr && m_pos == rhs.m_pos; }
    void increment() { m_path_iterator_increment(*this); }
    void decrement() { m_path_iterator_decrement(*this); }

    Path                    m_element;   // current element
    const Path*             m_path_ptr;  // path being iterated over
    string_type::size_type  m_pos;       // position of m_element in
                                         // m_path_ptr->m_pathname.
                                         // if m_element is implicit dot, m_pos is the
                                         // position of the last separator in the path.
                                         // end() iterator is indicated by
                                         // m_pos == m_path_ptr->m_pathname.size()
  }; // Path::iterator

  //------------------------------------------------------------------------------------//
  //                         class Path::reverse_iterator                               //
  //------------------------------------------------------------------------------------//

  class Path::reverse_iterator : public std::iterator<
                          std::bidirectional_iterator_tag, // iterator_category
                          Path,                            // value_type
                          long,                            // difference_type
                          const Path*,                     // pointer
                          Path&                            // reference
                        > {
public:
    explicit reverse_iterator(Path::iterator itr) :
        m_itr(itr) {
        if (itr != itr.m_path_ptr->begin())
            m_element = *--itr;
    }

    const Path& operator*() const { return dereference(); }
    const Path* operator->() const { return &dereference(); }

    reverse_iterator& operator++() {
        increment();
        return *this;
    }
    reverse_iterator operator++(int) {
        auto tmp = *this;
        increment();
        return tmp;
    }

    reverse_iterator& operator--() {
        decrement();
        return *this;
    }
    reverse_iterator operator--(int) {
        auto tmp = *this;
        decrement();
        return tmp;
    }

    bool operator==(const reverse_iterator& rhs) const { return equal(rhs);}
    bool operator!=(const reverse_iterator& rhs) const { return !equal(rhs);}

private:
    friend class io::Path;

    const Path& dereference() const { return m_element; }
    bool equal(const reverse_iterator& rhs) const { return m_itr == rhs.m_itr; }
    void increment() {
      --m_itr;
      if (m_itr != m_itr.m_path_ptr->begin()) {
        Path::iterator tmp = m_itr;
        m_element = *--tmp;
      }
    }

    void decrement() {
      m_element = *m_itr;
      ++m_itr;
    }

    Path::iterator m_itr;
    Path     m_element;

  }; // Path::reverse_iterator

  //------------------------------------------------------------------------------------//
  //                                                                                    //
  //                              non-member functions                                  //
  //                                                                                    //
  //------------------------------------------------------------------------------------//

  //  std::lexicographical_compare would infinitely recurse because path iterators
  //  yield paths, so provide a path aware version
  inline bool lexicographical_compare(Path::iterator first1,
                                      Path::iterator last1,
                                      Path::iterator first2,
                                      Path::iterator last2)
    { return detail::lex_compare(first1, last1, first2, last2) < 0; }

  inline bool operator==(const Path& lhs, const Path& rhs)              {return lhs.compare(rhs) == 0;}
  inline bool operator==(const Path& lhs, const Path::string_type& rhs) {return lhs.compare(rhs) == 0;}
  inline bool operator==(const Path::string_type& lhs, const Path& rhs) {return rhs.compare(lhs) == 0;}
  inline bool operator==(const Path& lhs, const Path::value_type* rhs)  {return lhs.compare(rhs) == 0;}
  inline bool operator==(const Path::value_type* lhs, const Path& rhs)  {return rhs.compare(lhs) == 0;}

  inline bool operator!=(const Path& lhs, const Path& rhs)              {return lhs.compare(rhs) != 0;}
  inline bool operator!=(const Path& lhs, const Path::string_type& rhs) {return lhs.compare(rhs) != 0;}
  inline bool operator!=(const Path::string_type& lhs, const Path& rhs) {return rhs.compare(lhs) != 0;}
  inline bool operator!=(const Path& lhs, const Path::value_type* rhs)  {return lhs.compare(rhs) != 0;}
  inline bool operator!=(const Path::value_type* lhs, const Path& rhs)  {return rhs.compare(lhs) != 0;}

  // TODO: why do == and != have additional overloads, but the others don't?

  inline bool operator<(const Path& lhs, const Path& rhs)  {return lhs.compare(rhs) < 0;}
  inline bool operator<=(const Path& lhs, const Path& rhs) {return !(rhs < lhs);}
  inline bool operator> (const Path& lhs, const Path& rhs) {return rhs < lhs;}
  inline bool operator>=(const Path& lhs, const Path& rhs) {return !(lhs < rhs);}

  inline std::size_t hash_value(const Path& x) noexcept
  {
# ifdef BOOST_WINDOWS_API
    std::size_t seed = 0;
    for(const Path::value_type* it = x.c_str(); *it; ++it)
      hash_combine(seed, *it == L'/' ? L'\\' : *it);
    return seed;
# else   // BOOST_POSIX_API
    return hash_range(x.native().begin(), x.native().end());
# endif
  }

  inline void swap(Path& lhs, Path& rhs) noexcept { lhs.swap(rhs); }

  inline Path operator/(const Path& lhs, const Path& rhs) {
    Path p = lhs;
    p /= rhs;
    return p;
  }

  inline Path operator/(Path&& lhs, const Path& rhs) {
    lhs /= rhs;
    return std::move(lhs);
  }

  //  inserters and extractors
  //    use boost::io::quoted() to handle spaces in paths
  //    use '&' as escape character to ease use for Windows paths

  template <class Char, class Traits>
  inline std::basic_ostream<Char, Traits>&
  operator<<(std::basic_ostream<Char, Traits>& os, const Path& p) {
    return os << ::io::quoted(p.template string<std::basic_string<Char> >(), static_cast<Char>('&'));
  }

  template <class Char, class Traits>
  inline std::basic_istream<Char, Traits>&
  operator>>(std::basic_istream<Char, Traits>& is, Path& p) {
    std::basic_string<Char> str;
    is >> ::io::quoted(str, static_cast<Char>('&'));
    p = str;
    return is;
  }

  //  name_checks

  //  These functions are holdovers from version 1. It isn't clear they have much
  //  usefulness, or how to generalize them for later versions.

  IO_DLL_PUBLIC bool portable_posix_name(const std::string & name);
  IO_DLL_PUBLIC bool windows_name(const std::string & name);
  IO_DLL_PUBLIC bool portable_name(const std::string & name);
  IO_DLL_PUBLIC bool portable_directory_name(const std::string & name);
  IO_DLL_PUBLIC bool portable_file_name(const std::string & name);
  IO_DLL_PUBLIC bool native(const std::string & name);

  namespace detail
  {
    //  For POSIX, is_directory_separator() and is_element_separator() are identical since
    //  a forward slash is the only valid directory separator and also the only valid
    //  element separator. For Windows, forward slash and back slash are the possible
    //  directory separators, but colon (example: "c:foo") is also an element separator.

    inline bool is_directory_separator(Path::value_type c) noexcept
    {
      return c == Path::separator
#     ifdef BOOST_WINDOWS_API
        || c == Path::preferred_separator
#     endif
      ;
    }
    inline bool is_element_separator(Path::value_type c) noexcept
    {
      return c == Path::separator
#     ifdef BOOST_WINDOWS_API
        || c == Path::preferred_separator || c == L':'
#     endif
      ;
    }
  }  // namespace detail

  //------------------------------------------------------------------------------------//
  //                  class path miscellaneous function implementations                 //
  //------------------------------------------------------------------------------------//

  inline Path::reverse_iterator Path::rbegin() const { return reverse_iterator(end()); }
  inline Path::reverse_iterator Path::rend() const   { return reverse_iterator(begin()); }

  inline bool Path::filename_is_dot() const {
    // implicit dot is tricky, so actually call filename(); see Path::filename() example
    // in reference.html
    Path p(filename());
    return p.size() == 1 && *p.c_str() == dot;
  }

  inline bool Path::filename_is_dot_dot() const {
    return size() >= 2 && m_pathname[size()-1] == dot && m_pathname[size()-2] == dot
      && (m_pathname.size() == 2 || detail::is_element_separator(m_pathname[size()-3]));
      // use detail::is_element_separator() rather than detail::is_directory_separator
      // to deal with "c:.." edge case on Windows when ':' acts as a separator
  }

//--------------------------------------------------------------------------------------//
//                     class path member template implementation                        //
//--------------------------------------------------------------------------------------//

  template <class InputIterator>
  Path& Path::append(InputIterator begin, InputIterator end) {
    if (begin == end)
      return *this;

    string_type::size_type sep_pos(m_append_separator_if_needed());
    std::basic_string<typename std::iterator_traits<InputIterator>::value_type> seq(begin, end);
    path_traits::convert(seq.c_str(), seq.c_str()+seq.size(), m_pathname);

    if (sep_pos)
      m_erase_redundant_separator(sep_pos);

    return *this;
  }

  template <class InputIterator>
  Path& Path::append(InputIterator begin, InputIterator end, const codecvt_type& cvt) {
    if (begin == end)
      return *this;

    string_type::size_type sep_pos(m_append_separator_if_needed());
    std::basic_string<typename std::iterator_traits<InputIterator>::value_type> seq(begin, end);
    path_traits::convert(seq.c_str(), seq.c_str()+seq.size(), m_pathname, cvt);

    if (sep_pos)
      m_erase_redundant_separator(sep_pos);

    return *this;
  }

  template <class Source>
  Path& Path::append(Source const& source) {
    if (path_traits::empty(source))
      return *this;

    string_type::size_type sep_pos(m_append_separator_if_needed());
    path_traits::dispatch(source, m_pathname);

    if (sep_pos)
      m_erase_redundant_separator(sep_pos);

    return *this;
  }

  template <class Source>
  Path& Path::append(Source const& source, const codecvt_type& cvt) {
    if (path_traits::empty(source))
      return *this;

    string_type::size_type sep_pos(m_append_separator_if_needed());
    path_traits::dispatch(source, m_pathname, cvt);

    if (sep_pos)
      m_erase_redundant_separator(sep_pos);

    return *this;
  }

//--------------------------------------------------------------------------------------//
//                     class path member template specializations                       //
//--------------------------------------------------------------------------------------//

  template <> inline
  std::string Path::string<std::string>() const
    { return string(); }

  template <> inline
  std::wstring Path::string<std::wstring>() const
    { return wstring(); }

  template <> inline
  std::string Path::string<std::string>(const codecvt_type& cvt) const
    { return string(cvt); }

  template <> inline
  std::wstring Path::string<std::wstring>(const codecvt_type& cvt) const
    { return wstring(cvt); }

  template <> inline
  std::string Path::generic_string<std::string>() const
    { return generic_string(); }

  template <> inline
  std::wstring Path::generic_string<std::wstring>() const
    { return generic_wstring(); }

  template <> inline
  std::string Path::generic_string<std::string>(const codecvt_type& cvt) const
    { return generic_string(cvt); }

  template <> inline
  std::wstring Path::generic_string<std::wstring>(const codecvt_type& cvt) const
    { return generic_wstring(cvt); }

}  // namespace io

// Path hasher for std
namespace std {

template <> struct hash<io::Path> {
    size_t operator()(const io::Path& x) const {
        return io::hash_value(x);
    }
};

} // namespace std

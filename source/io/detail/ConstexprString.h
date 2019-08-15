#pragma once

#include <limits>

#define IO_MSVC_MAX_MACRO_WORKAROUND

namespace io {
namespace detail {

using size_type = std::size_t;
const auto NPOS = std::numeric_limits<size_type>::max IO_MSVC_MAX_MACRO_WORKAROUND ();

constexpr std::size_t length_impl(char const* str, std::size_t i) {
    return str ? (str[i] ? 1 + length_impl(str, i + 1) : 0) : 0;
}

constexpr std::size_t length(char const* str) {
    return length_impl(str, 0);
}

static_assert(length("") == 0, "");
static_assert(length("abc") == 3, "");

template<typename S, typename V>
constexpr bool equals_one_of(S s, V v) {
    return s == v;
}

template<typename S, typename V, typename... T>
constexpr bool equals_one_of(S s, V v, T... t) {
    return s == v || equals_one_of(s, t...);
}

static_assert(equals_one_of('a', 'a'), "");
static_assert(!equals_one_of('a', 'b'), "");
static_assert(equals_one_of('a', 'a', 'b', 'c', 'd'), "");
static_assert(!equals_one_of('a', 'b', 'c', 'd'), "");

template<typename... C>
constexpr size_type rfind_impl(const char* const str, size_type i, C... c) {
    return i == 0 ? (equals_one_of(str[0], c...) ? 0 : NPOS) :
                    (equals_one_of(str[i], c...) ? i : rfind_impl(str, i - 1, c...));
}

template<typename... C>
constexpr size_type rfind(const char* const str, C... c) {
    return rfind_impl(str, length(str), c...);
}

constexpr const char* extract_file_name_from_path(char const* str) {
    return str + (rfind(str, '/', '\\') + 1);
}

// TODO: extract to tests????
static_assert(rfind("/aaa/bb/cc", '/') == 7, "");
static_assert(rfind("aaabbcc", '/') == NPOS, "");
static_assert(rfind("/aaabbcc", '/') == 0, "");

static_assert(rfind("/aaa/bb/cc", '/', '\\') == 7, "");
static_assert(rfind("/aaa/bb\\cc", '/', '\\') == 7, "");
static_assert(rfind("aaabbcc", '/', '\\') == NPOS, "");
static_assert(rfind("/aaabbcc", '/', '\\') == 0, "");

// Proof that 'extract_file_name_from_path' works in compile time (static_assert works only in compile time)
#if defined( _MSC_VER) || defined(__clang__) || (__GNUC__ >= 7)
// GCC versions 5 and 6 have known bug that (ptr + i) stops to become constexpr,
// but nevertheless call to extract_file_name_from_path is optimized under -O2 to simple output of (str + offset)
static_assert(extract_file_name_from_path(__FILE__) != nullptr, "");
#endif

} // namespace detail
} // namespace io
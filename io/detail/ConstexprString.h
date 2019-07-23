#include <limits>

namespace io {
namespace detail {

using size_type = std::size_t;
const auto NPOS = std::numeric_limits<size_type>::max();

constexpr std::size_t length_impl(char const* str, std::size_t i) {
    return str ? (str[i] ? 1 + length_impl(str, i + 1) : 0) : 0;
}

constexpr std::size_t length(char const* str) {
    return length_impl(str, 0);
}

constexpr size_type rfind_impl(const char* const str, char c, size_type i) {
    return i == 0 ? (str[0] == c ? 0 : NPOS) :
                    (str[i] == c ? i : rfind_impl(str, c, i - 1));
}

constexpr size_type rfind(const char* const str, char c) {
    return rfind_impl(str, c, length(str));

}

constexpr const char* extract_file_name_from_path(char const* str) {
    return str + rfind(str, '/') + 1;
}

static_assert(length("") == 0, "");
static_assert(length("abc") == 3, "");

// TODO: extract to tests????
static_assert(rfind("/aaa/bb/cc", '/') == 7, "");
static_assert(rfind("aaabbcc", '/') == NPOS, "");
static_assert(rfind("/aaabbcc", '/') == 0, "");

// Proof that 'extract_file_name_from_path' works in compile time (static_assert works only in compile time)
#if defined( _MSC_VER) || defined(__clang__)
static_assert(extract_file_name_from_path(__FILE__) != nullptr, "");
#endif

} // namespace detail
} // namespace io

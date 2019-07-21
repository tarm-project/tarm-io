#include <limits>

namespace io {
namespace detail {

using size_type = std::size_t;

struct SymbolOutOfRange{
    SymbolOutOfRange(size_type, size_type){
    }
};

constexpr size_type check_in_range(size_type i, size_type len)
{
    return i >= len ? throw SymbolOutOfRange(i, len) : i;
}

class ConstexprString
{
public:
    static const auto NPOS = std::numeric_limits<size_type>::max();

    template<size_type N>
    constexpr ConstexprString( const char(&arr)[N], size_t offset = 0) :
        m_begin(arr + offset), m_size(N - 1) {
        static_assert( N >= 1, "not a string literal");
    }

    constexpr char operator[](size_type i)  const {
        return check_in_range(i, m_size), m_begin[i];
    }

    constexpr size_type size()  const {
        return m_size;
    }

    constexpr const char* const begin() const {
        return m_begin;
    }

private:
    const char* const m_begin;
    const size_type m_size;
};

constexpr size_type rfind(ConstexprString str, char c, size_type i = 0) {
    return i == str.size() ? ConstexprString::NPOS :
        str[str.size() - i - 1] == c ? str.size() - i - 1 :
                                       rfind(str, c, i + 1);
}

constexpr const char* extract_file_name_from_path(ConstexprString str) {
    const auto v = rfind(str, '/');
    return str.begin() + (v == ConstexprString::NPOS ? 0 : v + 1);

}

// TODO: extract to tests????
static_assert(rfind("/aaa/bb/cc", '/') == 7, "!!!");
static_assert(rfind("aaabbcc", '/') == -1, "!!!");
static_assert(rfind("/aaabbcc", '/') == 0, "!!!");

// Proof that 'extract_file_name_from_path' works in compile time (static_assert works only in compile time)
static_assert(extract_file_name_from_path(__FILE__) != nullptr, "!!!");

} // namespace detail
} // namespace io

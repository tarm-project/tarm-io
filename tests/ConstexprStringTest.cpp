#include "io/detail/ConstexprString.h"

using namespace io::detail;

static_assert(length("") == 0, "");
static_assert(length("abc") == 3, "");

static_assert(equals_one_of('a', 'a'), "");
static_assert(!equals_one_of('a', 'b'), "");
static_assert(equals_one_of('a', 'a', 'b', 'c', 'd'), "");
static_assert(!equals_one_of('a', 'b', 'c', 'd'), "");

static_assert(rfind("/aaa/bb/cc", '/') == 7, "");
static_assert(rfind("aaabbcc", '/') == NPOS, "");
static_assert(rfind("/aaabbcc", '/') == 0, "");

static_assert(rfind("/aaa/bb/cc", '/', '\\') == 7, "");
static_assert(rfind("/aaa/bb\\cc", '/', '\\') == 7, "");
static_assert(rfind("aaabbcc", '/', '\\') == NPOS, "");
static_assert(rfind("/aaabbcc", '/', '\\') == 0, "");

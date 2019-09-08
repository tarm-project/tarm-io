#include "UTCommon.h"

#include "io/detail/ConstexprString.h"

#include "io/Path.h"

#include "io/Utf8CodecvtFacet.h"

#include <iterator>
#include <string>
#include <locale>
#include <cwchar>  // for mbstate_t
#include <functional>

// Workaround for GTest printers bug and path
// https://github.com/google/googlemock/issues/170
namespace io {

void PrintTo(const io::Path& path, std::ostream* os) {
    *os << path;
}

} // namespace io

// TODO: fixme
#ifdef BOOST_WINDOWS_API
# define BOOST_DIR_SEP "\\"
#else
# define BOOST_DIR_SEP "/"
#endif

using io::Path;

struct PathTest : public testing::Test,
                  public LogRedirector {
protected:
    PathTest() :
        m_s("string"),
        m_ws(L"wstring") {
        m_l.push_back('s');
        m_l.push_back('t');
        m_l.push_back('r');
        m_l.push_back('i');
        m_l.push_back('n');
        m_l.push_back('g');

        m_wl.push_back(L'w');
        m_wl.push_back(L's');
        m_wl.push_back(L't');
        m_wl.push_back(L'r');
        m_wl.push_back(L'i');
        m_wl.push_back(L'n');
        m_wl.push_back(L'g');

        m_v.push_back('f');
        m_v.push_back('u');
        m_v.push_back('z');

        m_wv.push_back(L'w');
        m_wv.push_back(L'f');
        m_wv.push_back(L'u');
        m_wv.push_back(L'z');
    }

    std::string m_s;
    std::wstring m_ws;
    std::list<char> m_l;      // see main() for initialization to s, t, r, i, n, g
    std::list<wchar_t> m_wl;  // see main() for initialization to w, s, t, r, i, n, g
    std::vector<char> m_v;      // see main() for initialization to f, u, z
    std::vector<wchar_t> m_wv;  // see main() for initialization to w, f, u, z

    Path m_x;
    Path m_y;
};

TEST_F(PathTest, constructors) {
    Path x0;                                           // default constructor
    EXPECT_EQ(x0, L"");
    EXPECT_EQ(x0.native().size(), 0U);

    Path x1(m_l.begin(), m_l.end());                       // iterator range char
    EXPECT_EQ(x1, L"string");
    EXPECT_EQ(x1.native().size(), 6U);

    Path x2(x1);                                       // copy constructor
    EXPECT_EQ(x2, L"string");
    EXPECT_EQ(x2.native().size(), 6U);

    Path x3(m_wl.begin(), m_wl.end());                     // iterator range wchar_t
    EXPECT_EQ(x3, L"wstring");
    EXPECT_EQ(x3.native().size(), 7U);

    // contiguous containers
    Path x4(std::string("std::string"));                    // std::string
    EXPECT_EQ(x4, L"std::string");
    EXPECT_EQ(x4.native().size(), 11U);

    Path x5(std::wstring(L"std::wstring"));                 // std::wstring
    EXPECT_EQ(x5, L"std::wstring");
    EXPECT_EQ(x5.native().size(), 12U);

    Path x4v(m_v);                                       // std::vector<char>
    EXPECT_EQ(x4v, L"fuz");
    EXPECT_EQ(x4v.native().size(), 3U);

    Path x5v(m_wv);                                      // std::vector<wchar_t>
    EXPECT_EQ(x5v, L"wfuz");
    EXPECT_EQ(x5v.native().size(), 4U);

    Path x6("array char");                             // array char
    EXPECT_EQ(x6, L"array char");
    EXPECT_EQ(x6.native().size(), 10U);

    Path x7(L"array wchar_t");                         // array wchar_t
    EXPECT_EQ(x7, L"array wchar_t");
    EXPECT_EQ(x7.native().size(), 13U);

    char char_array[100];
    std::strcpy(char_array, "big array char");
    Path x6o(char_array);                              // array char, only partially full
    EXPECT_EQ(x6o, L"big array char");
    EXPECT_EQ(x6o.native().size(), 14U);

    wchar_t wchar_array[100];
    std::wcscpy(wchar_array, L"big array wchar_t");
    Path x7o(wchar_array);                             // array char, only partially full
    EXPECT_EQ(x7o, L"big array wchar_t");
    EXPECT_EQ(x7o.native().size(), 17U);

    Path x8(m_s.c_str());                                // const char* null terminated
    EXPECT_EQ(x8, L"string");
    EXPECT_EQ(x8.native().size(), 6U);

    Path x9(m_ws.c_str());                               // const wchar_t* null terminated
    EXPECT_EQ(x9, L"wstring");
    EXPECT_EQ(x9.native().size(), 7U);

    Path x8nc(const_cast<char*>(m_s.c_str()));           // char* null terminated
    EXPECT_EQ(x8nc, L"string");
    EXPECT_EQ(x8nc.native().size(), 6U);

    Path x9nc(const_cast<wchar_t*>(m_ws.c_str()));       // wchar_t* null terminated
    EXPECT_EQ(x9nc, L"wstring");
    EXPECT_EQ(x9nc.native().size(), 7U);

    // non-contiguous containers
    Path x10(m_l);                                       // std::list<char>
    EXPECT_EQ(x10, L"string");
    EXPECT_EQ(x10.native().size(), 6U);

    Path xll(m_wl);                                      // std::list<wchar_t>
    EXPECT_EQ(xll, L"wstring");
    EXPECT_EQ(xll.native().size(), 7U);
}

TEST_F(PathTest, assignments) {
    m_x = Path("yet another path");                      // another path
    EXPECT_EQ(m_x, L"yet another path");
    EXPECT_EQ(m_x.native().size(), 16U);

    m_x = m_x;                                             // self-assignment
    EXPECT_EQ(m_x, L"yet another path");
    EXPECT_EQ(m_x.native().size(), 16U);

    m_x.assign(m_l.begin(), m_l.end());                      // iterator range char
    EXPECT_EQ(m_x, L"string");

    m_x.assign(m_wl.begin(), m_wl.end());                    // iterator range wchar_t
    EXPECT_EQ(m_x, L"wstring");

    m_x = std::string("std::string");                         // container char
    EXPECT_EQ(m_x, L"std::string");

    m_x = std::wstring(L"std::wstring");                      // container wchar_t
    EXPECT_EQ(m_x, L"std::wstring");

    m_x = "array char";                                  // array char
    EXPECT_EQ(m_x, L"array char");

    m_x = L"array wchar";                                // array wchar_t
    EXPECT_EQ(m_x, L"array wchar");

    m_x = m_s.c_str();                                     // const char* null terminated
    EXPECT_EQ(m_x, L"string");

    m_x = m_ws.c_str();                                    // const wchar_t* null terminated
    EXPECT_EQ(m_x, L"wstring");
}

TEST_F(PathTest, move_construction_and_assignment) {
    Path from("long enough to avoid small object optimization");
    Path to(std::move(from));
    EXPECT_TRUE(to == "long enough to avoid small object optimization");
    EXPECT_TRUE(from.empty());

    Path from2("long enough to avoid small object optimization");
    Path to2;
    to2 = std::move(from2);
    EXPECT_TRUE(to2 == "long enough to avoid small object optimization");
    EXPECT_TRUE(from2.empty());

}

TEST_F(PathTest, appends) {
// TODO: fixme
# ifdef BOOST_WINDOWS_API
#   define BOOST_FS_FOO L"/foo\\"
# else   // POSIX paths
#   define BOOST_FS_FOO L"/foo/"
# endif

    m_x = "/foo";
    m_x /= Path("");                                      // empty path
    EXPECT_EQ(m_x, L"/foo");

    m_x = "/foo";
    m_x /= Path("/");                                     // slash path
    EXPECT_EQ(m_x, L"/foo/");

    m_x = "/foo";
    m_x /= Path("/boo");                                  // slash path
    EXPECT_EQ(m_x, L"/foo/boo");

    m_x = "/foo";
    m_x /= m_x;                                             // self-append
    EXPECT_EQ(m_x, L"/foo/foo");

    m_x = "/foo";
    m_x /= Path("yet another path");                      // another path
    EXPECT_EQ(m_x, BOOST_FS_FOO L"yet another path");

    m_x = "/foo";
    m_x.append(m_l.begin(), m_l.end());                      // iterator range char
    EXPECT_EQ(m_x, BOOST_FS_FOO L"string");

    m_x = "/foo";
    m_x.append(m_wl.begin(), m_wl.end());                    // iterator range wchar_t
    EXPECT_EQ(m_x, BOOST_FS_FOO L"wstring");

    m_x = "/foo";
    m_x /= std::string("std::string");                         // container char
    EXPECT_EQ(m_x, BOOST_FS_FOO L"std::string");

    m_x = "/foo";
    m_x /= std::wstring(L"std::wstring");                      // container wchar_t
    EXPECT_EQ(m_x, BOOST_FS_FOO L"std::wstring");

    m_x = "/foo";
    m_x /= "array char";                                  // array char
    EXPECT_EQ(m_x, BOOST_FS_FOO L"array char");

    m_x = "/foo";
    m_x /= L"array wchar";                                // array wchar_t
    EXPECT_EQ(m_x, BOOST_FS_FOO L"array wchar");

    m_x = "/foo";
    m_x /= m_s.c_str();                                     // const char* null terminated
    EXPECT_EQ(m_x, BOOST_FS_FOO L"string");

    m_x = "/foo";
    m_x /= m_ws.c_str();                                    // const wchar_t* null terminated
    EXPECT_EQ(m_x, BOOST_FS_FOO L"wstring");
}

TEST_F(PathTest, concats) {
    m_x = "/foo";
    m_x += Path("");                                      // empty path
    EXPECT_EQ(m_x, L"/foo");

    m_x = "/foo";
    m_x += Path("/");                                     // slash path
    EXPECT_EQ(m_x, L"/foo/");

    m_x = "/foo";
    m_x += Path("boo");                                  // slash path
    EXPECT_EQ(m_x, L"/fooboo");

    m_x = "foo";
    m_x += m_x;                                             // self-append
    EXPECT_EQ(m_x, L"foofoo");

    m_x = "foo-";
    m_x += Path("yet another path");                      // another path
    EXPECT_EQ(m_x, L"foo-yet another path");

    m_x = "foo-";
    m_x.concat(m_l.begin(), m_l.end());                      // iterator range char
    EXPECT_EQ(m_x, L"foo-string");

    m_x = "foo-";
    m_x.concat(m_wl.begin(), m_wl.end());                    // iterator range wchar_t
    EXPECT_EQ(m_x, L"foo-wstring");

    m_x = "foo-";
    m_x += std::string("std::string");                         // container char
    EXPECT_EQ(m_x, L"foo-std::string");

    m_x = "foo-";
    m_x += std::wstring(L"std::wstring");                      // container wchar_t
    EXPECT_EQ(m_x, L"foo-std::wstring");

    m_x = "foo-";
    m_x += "array char";                                  // array char
    EXPECT_EQ(m_x, L"foo-array char");

    m_x = "foo-";
    m_x += L"array wchar";                                // array wchar_t
    EXPECT_EQ(m_x, L"foo-array wchar");

    m_x = "foo-";
    m_x += m_s.c_str();                                     // const char* null terminated
    EXPECT_EQ(m_x, L"foo-string");

    m_x = "foo-";
    m_x += m_ws.c_str();                                    // const wchar_t* null terminated
    EXPECT_EQ(m_x, L"foo-wstring");

    m_x = "foo-";
    m_x += 'x';                                           // char
    EXPECT_EQ(m_x, L"foo-x");

    m_x = "foo-";
    m_x += L'x';                                          // wchar
    EXPECT_EQ(m_x, L"foo-x");
}

TEST_F(PathTest, observers) {
    Path p0("abc");

    EXPECT_TRUE(p0.native().size() == 3);
    EXPECT_TRUE(p0.size() == 3);
    EXPECT_TRUE(p0.string() == "abc");
    EXPECT_TRUE(p0.string().size() == 3);
    EXPECT_TRUE(p0.wstring() == L"abc");
    EXPECT_TRUE(p0.wstring().size() == 3);

    p0 = "";
    EXPECT_TRUE(p0.native().size() == 0);
    EXPECT_TRUE(p0.size() == 0);

// TODO: fixme
# ifdef BOOST_WINDOWS_API
    Path p("abc\\def/ghi");

    EXPECT_TRUE(std::wstring(p.c_str()) == L"abc\\def/ghi");

    EXPECT_TRUE(p.string() == "abc\\def/ghi");
    EXPECT_TRUE(p.wstring() == L"abc\\def/ghi");

    EXPECT_TRUE(p.generic_path().string() == "abc/def/ghi");
    EXPECT_TRUE(p.generic_string() == "abc/def/ghi");
    EXPECT_TRUE(p.generic_wstring() == L"abc/def/ghi");

    EXPECT_TRUE(p.generic_string<std::string>() == "abc/def/ghi");
    EXPECT_TRUE(p.generic_string<std::wstring>() == L"abc/def/ghi");
    EXPECT_TRUE(p.generic_string<Path::string_type>() == L"abc/def/ghi");
# else  // BOOST_POSIX_API
    Path p("abc\\def/ghi");

    EXPECT_TRUE(std::string(p.c_str()) == "abc\\def/ghi");

    EXPECT_TRUE(p.string() == "abc\\def/ghi");
    EXPECT_TRUE(p.wstring() == L"abc\\def/ghi");

    EXPECT_TRUE(p.generic_path().string() == "abc\\def/ghi");
    EXPECT_TRUE(p.generic_string() == "abc\\def/ghi");
    EXPECT_TRUE(p.generic_wstring() == L"abc\\def/ghi");

    EXPECT_TRUE(p.generic_string<std::string>() == "abc\\def/ghi");
    EXPECT_TRUE(p.generic_string<std::wstring>() == L"abc\\def/ghi");
    EXPECT_TRUE(p.generic_string<Path::string_type>() == "abc\\def/ghi");
# endif
}

TEST_F(PathTest, relationals) {
    std::hash<Path> hash;

# ifdef BOOST_WINDOWS_API
    // this is a critical use case to meet user expectations
    EXPECT_TRUE(Path("c:\\abc") == Path("c:/abc"));
    EXPECT_TRUE(hash(Path("c:\\abc")) == hash(Path("c:/abc")));
# endif

    const Path p("bar");
    const Path p2("baz");

    EXPECT_TRUE(!(p < p));
    EXPECT_TRUE(p < p2);
    EXPECT_TRUE(!(p2 < p));
    EXPECT_TRUE(p < "baz");
    EXPECT_TRUE(p < std::string("baz"));
    EXPECT_TRUE(p < L"baz");
    EXPECT_TRUE(p < std::wstring(L"baz"));
    EXPECT_TRUE(!("baz" < p));
    EXPECT_TRUE(!(std::string("baz") < p));
    EXPECT_TRUE(!(L"baz" < p));
    EXPECT_TRUE(!(std::wstring(L"baz") < p));

    EXPECT_TRUE(p == p);
    EXPECT_TRUE(!(p == p2));
    EXPECT_TRUE(!(p2 == p));
    EXPECT_TRUE(p2 == "baz");
    EXPECT_TRUE(p2 == std::string("baz"));
    EXPECT_TRUE(p2 == L"baz");
    EXPECT_TRUE(p2 == std::wstring(L"baz"));
    EXPECT_TRUE("baz" == p2);
    EXPECT_TRUE(std::string("baz") == p2);
    EXPECT_TRUE(L"baz" == p2);
    EXPECT_TRUE(std::wstring(L"baz") == p2);

    EXPECT_TRUE(hash(p) == hash(p));
    EXPECT_TRUE(hash(p) != hash(p2)); // Not strictly required, but desirable

    EXPECT_TRUE(!(p != p));
    EXPECT_TRUE(p != p2);
    EXPECT_TRUE(p2 != p);

    EXPECT_TRUE(p <= p);
    EXPECT_TRUE(p <= p2);
    EXPECT_TRUE(!(p2 <= p));

    EXPECT_TRUE(!(p > p));
    EXPECT_TRUE(!(p > p2));
    EXPECT_TRUE(p2 > p);

    EXPECT_TRUE(p >= p);
    EXPECT_TRUE(!(p >= p2));
    EXPECT_TRUE(p2 >= p);
}

TEST_F(PathTest, inserter_and_extractor) {
    Path p1("foo bar");  // verify space in Path roundtrips per ticket #3863
    Path p2;

    std::stringstream ss;

    EXPECT_TRUE(p1 != p2);
    ss << p1;
    ss >> p2;
    EXPECT_TRUE(p1 == p2);

    Path wp1(L"foo bar");
    Path wp2;

    std::wstringstream wss;

    EXPECT_TRUE(wp1 != wp2);
    wss << wp1;
    wss >> wp2;
    EXPECT_TRUE(wp1 == wp2);
}

TEST_F(PathTest, other_non_members) {
    Path p1("foo");
    Path p2("bar");

    //  operator /
    EXPECT_TRUE(p1 / p2 == Path("foo/bar").make_preferred());
    EXPECT_TRUE("foo" / p2 == Path("foo/bar").make_preferred());
    EXPECT_TRUE(L"foo" / p2 == Path("foo/bar").make_preferred());
    EXPECT_TRUE(std::string("foo") / p2 == Path("foo/bar").make_preferred());
    EXPECT_TRUE(std::wstring(L"foo") / p2 == Path("foo/bar").make_preferred());
    EXPECT_TRUE(p1 / "bar" == Path("foo/bar").make_preferred());
    EXPECT_TRUE(p1 / L"bar" == Path("foo/bar").make_preferred());
    EXPECT_TRUE(p1 / std::string("bar") == Path("foo/bar").make_preferred());
    EXPECT_TRUE(p1 / std::wstring(L"bar") == Path("foo/bar").make_preferred());

    swap(p1, p2);

    EXPECT_TRUE(p1 == "bar");
    EXPECT_TRUE(p2 == "foo");

    EXPECT_TRUE(!Path("").filename_is_dot());
    EXPECT_TRUE(!Path("").filename_is_dot_dot());
    EXPECT_TRUE(!Path("..").filename_is_dot());
    EXPECT_TRUE(!Path(".").filename_is_dot_dot());
    EXPECT_TRUE(!Path("...").filename_is_dot_dot());
    EXPECT_TRUE(Path(".").filename_is_dot());
    EXPECT_TRUE(Path("..").filename_is_dot_dot());
    EXPECT_TRUE(Path("/.").filename_is_dot());
    EXPECT_TRUE(Path("/..").filename_is_dot_dot());
    EXPECT_TRUE(!Path("a.").filename_is_dot());
    EXPECT_TRUE(!Path("a..").filename_is_dot_dot());

    // edge cases
    EXPECT_TRUE(Path("foo/").filename() == Path("."));
    EXPECT_TRUE(Path("foo/").filename_is_dot());
    EXPECT_TRUE(Path("/").filename() == Path("/"));
    EXPECT_TRUE(!Path("/").filename_is_dot());
// TODO: fixme
# ifdef BOOST_WINDOWS_API
    EXPECT_TRUE(Path("c:.").filename() == Path("."));
    EXPECT_TRUE(Path("c:.").filename_is_dot());
    EXPECT_TRUE(Path("c:/").filename() == Path("/"));
    EXPECT_TRUE(!Path("c:\\").filename_is_dot());
# else
    EXPECT_TRUE(Path("c:.").filename() == Path("c:."));
    EXPECT_TRUE(!Path("c:.").filename_is_dot());
    EXPECT_TRUE(Path("c:/").filename() == Path("."));
    EXPECT_TRUE(Path("c:/").filename_is_dot());
# endif

    // check that the implementation code to make the edge cases above work right
    // doesn't cause some non-edge cases to fail
    EXPECT_TRUE(Path("c:").filename() != Path("."));
    EXPECT_TRUE(!Path("c:").filename_is_dot());
}

TEST_F(PathTest, iterator) {
    Path itr_ck = "";
    Path::const_iterator itr = itr_ck.begin();
    EXPECT_TRUE(itr == itr_ck.end());

    itr_ck = "/";
    itr = itr_ck.begin();
    EXPECT_TRUE(itr->string() == "/");
    EXPECT_TRUE(++itr == itr_ck.end());
    EXPECT_TRUE((--itr)->string() == "/");

    itr_ck = "foo";
    EXPECT_TRUE(*itr_ck.begin() == std::string("foo"));
    EXPECT_TRUE(std::next(itr_ck.begin()) == itr_ck.end());
    EXPECT_TRUE(*std::prev(itr_ck.end()) == std::string("foo"));
    EXPECT_TRUE(std::prev(itr_ck.end()) == itr_ck.begin());

    itr_ck = Path("/foo");
    EXPECT_TRUE((itr_ck.begin())->string() == "/");
    EXPECT_TRUE(*std::next(itr_ck.begin()) == std::string("foo"));
    EXPECT_TRUE(std::next(std::next(itr_ck.begin())) == itr_ck.end());
    EXPECT_TRUE(std::next(itr_ck.begin()) == std::prev(itr_ck.end()));
    EXPECT_TRUE(*std::prev(itr_ck.end()) == std::string("foo"));
    EXPECT_TRUE(*std::prev(std::prev(itr_ck.end())) == std::string("/"));
    EXPECT_TRUE(std::prev(std::prev(itr_ck.end())) == itr_ck.begin());

    itr_ck = "/foo/bar";
    itr = itr_ck.begin();
    EXPECT_TRUE(itr->string() == "/");
    EXPECT_TRUE(*++itr == std::string("foo"));
    EXPECT_TRUE(*++itr == std::string("bar"));
    EXPECT_TRUE(++itr == itr_ck.end());
    EXPECT_EQ(*--itr, "bar");
    EXPECT_EQ(*--itr, "foo");
    EXPECT_EQ(*--itr, "/");

    itr_ck = "../f"; // previously failed due to short name bug
    itr = itr_ck.begin();
    EXPECT_EQ(itr->string(), "..");
    EXPECT_EQ(*++itr, "f");
    EXPECT_TRUE(++itr == itr_ck.end());
    EXPECT_EQ(*--itr, "f");
    EXPECT_EQ(*--itr, "..");

    // POSIX says treat "/foo/bar/" as "/foo/bar/."
    itr_ck = "/foo/bar/";
    itr = itr_ck.begin();
    EXPECT_EQ(itr->string(), "/");
    EXPECT_EQ(*++itr, "foo");
    EXPECT_TRUE(itr != itr_ck.end());
    EXPECT_EQ(*++itr, "bar");
    EXPECT_TRUE(itr != itr_ck.end());
    EXPECT_EQ(*++itr, ".");
    EXPECT_TRUE(itr != itr_ck.end());  // verify the . isn't also seen as end()
    EXPECT_TRUE(++itr == itr_ck.end());
    EXPECT_EQ(*--itr, ".");
    EXPECT_EQ(*--itr, "bar");
    EXPECT_EQ(*--itr, "foo");
    EXPECT_EQ(*--itr, "/");

    // POSIX says treat "/f/b/" as "/f/b/."
    itr_ck = "/f/b/";
    itr = itr_ck.begin();
    EXPECT_EQ(itr->string(), "/");
    EXPECT_EQ(*++itr, "f");
    EXPECT_EQ(*++itr, "b");
    EXPECT_EQ(*++itr, ".");
    EXPECT_TRUE(itr != itr_ck.end());  // verify the . isn't also seen as end()
    EXPECT_TRUE(++itr == itr_ck.end());
    EXPECT_EQ(*--itr, ".");
    EXPECT_EQ(*--itr, "b");
    EXPECT_EQ(*--itr, "f");
    EXPECT_EQ(*--itr, "/");

    // POSIX says treat "a/b/" as "a/b/."
    // Although similar to the prior test case, this failed the ". isn't end" test due to
    // a bug while the prior case did not fail.
    itr_ck = "a/b/";
    itr = itr_ck.begin();
    EXPECT_EQ(*itr, "a");
    EXPECT_EQ(*++itr, "b");
    EXPECT_EQ(*++itr, ".");
    EXPECT_TRUE(itr != itr_ck.end());  // verify the . isn't also seen as end()
    EXPECT_TRUE(++itr == itr_ck.end());
    EXPECT_EQ(*--itr, ".");
    EXPECT_EQ(*--itr, "b");
    EXPECT_EQ(*--itr, "a");

    itr_ck = "//net";
    itr = itr_ck.begin();
    // two leading slashes are permitted by POSIX (as implementation defined),
    // while for Windows it is always well defined (as a network name)
    EXPECT_EQ(itr->string(), "//net");
    EXPECT_TRUE(++itr == itr_ck.end());
    EXPECT_EQ(*--itr, "//net");

    itr_ck = "//net/";
    itr = itr_ck.begin();
    EXPECT_EQ(itr->string(), "//net");
    EXPECT_EQ(*++itr, "/");
    EXPECT_TRUE(++itr == itr_ck.end());
    EXPECT_EQ(*--itr, "/");
    EXPECT_EQ(*--itr, "//net");

    itr_ck = "//foo///bar///";
    itr = itr_ck.begin();
    EXPECT_EQ(itr->string(), "//foo");
    EXPECT_EQ(*++itr, "/");
    EXPECT_EQ(*++itr, "bar");
    EXPECT_EQ(*++itr, ".");
    EXPECT_TRUE(++itr == itr_ck.end());
    EXPECT_EQ(*--itr, ".");
    EXPECT_EQ(*--itr, "bar");
    EXPECT_EQ(*--itr, "/");
    EXPECT_EQ(*--itr, "//foo");

    itr_ck = "///foo///bar///";
    itr = itr_ck.begin();
    // three or more leading slashes are to be treated as a single slash
    EXPECT_EQ(itr->string(), "/");
    EXPECT_EQ(*++itr, "foo");
    EXPECT_EQ(*++itr, "bar");
    EXPECT_EQ(*++itr, ".");
    EXPECT_TRUE(++itr == itr_ck.end());
    EXPECT_EQ(*--itr, ".");
    EXPECT_EQ(*--itr, "bar");
    EXPECT_EQ(*--itr, "foo");
    EXPECT_EQ(*--itr, "/");

// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
        itr_ck = "c:/";
        itr = itr_ck.begin();
        EXPECT_EQ(itr->string(), "c:");
        EXPECT_EQ(*++itr, std::string("/"));
        EXPECT_TRUE(++itr == itr_ck.end());
        EXPECT_EQ(*--itr, "/");
        EXPECT_EQ(*--itr, "c:");

        itr_ck = "c:\\";
        itr = itr_ck.begin();
        EXPECT_EQ(itr->string(), "c:");
        EXPECT_EQ(*++itr, "/");  // test that iteration returns generic format
        EXPECT_TRUE(++itr == itr_ck.end());
        EXPECT_EQ(*--itr, "/");  // test that iteration returns generic format
        EXPECT_EQ(*--itr, "c:");

        itr_ck = "c:/foo";
        itr = itr_ck.begin();
        EXPECT_TRUE(*itr == std::string("c:"));
        EXPECT_TRUE(*++itr == std::string("/"));
        EXPECT_TRUE(*++itr == std::string("foo"));
        EXPECT_TRUE(++itr == itr_ck.end());
        EXPECT_TRUE(*--itr == std::string("foo"));
        EXPECT_TRUE((--itr)->string() == "/");
        EXPECT_TRUE(*--itr == std::string("c:"));

        itr_ck = "c:\\foo";
        itr = itr_ck.begin();
        EXPECT_TRUE(*itr == std::string("c:"));
        EXPECT_TRUE(*++itr == std::string("\\"));
        EXPECT_TRUE(*++itr == std::string("foo"));
        EXPECT_TRUE(++itr == itr_ck.end());
        EXPECT_TRUE(*--itr == std::string("foo"));
        EXPECT_TRUE(*--itr == std::string("\\"));
        EXPECT_TRUE(*--itr == std::string("c:"));

        itr_ck = "\\\\\\foo\\\\\\bar\\\\\\";
        itr = itr_ck.begin();
        // three or more leading slashes are to be treated as a single slash
        EXPECT_EQ(itr->string(), "/");
        EXPECT_EQ(*++itr, "foo");
        EXPECT_EQ(*++itr, "bar");
        EXPECT_EQ(*++itr, ".");
        EXPECT_TRUE(++itr == itr_ck.end());
        EXPECT_EQ(*--itr, ".");
        EXPECT_EQ(*--itr, "bar");
        EXPECT_EQ(*--itr, "foo");
        EXPECT_EQ(*--itr, "/");

        itr_ck = "c:foo";
        itr = itr_ck.begin();
        EXPECT_TRUE(*itr == std::string("c:"));
        EXPECT_TRUE(*++itr == std::string("foo"));
        EXPECT_TRUE(++itr == itr_ck.end());
        EXPECT_TRUE(*--itr == std::string("foo"));
        EXPECT_TRUE(*--itr == std::string("c:"));

        itr_ck = "c:foo/";
        itr = itr_ck.begin();
        EXPECT_TRUE(*itr == std::string("c:"));
        EXPECT_TRUE(*++itr == std::string("foo"));
        EXPECT_TRUE(*++itr == std::string("."));
        EXPECT_TRUE(++itr == itr_ck.end());
        EXPECT_TRUE(*--itr == std::string("."));
        EXPECT_TRUE(*--itr == std::string("foo"));
        EXPECT_TRUE(*--itr == std::string("c:"));

        itr_ck = Path("c:");
        EXPECT_TRUE(*itr_ck.begin() == std::string("c:"));
        EXPECT_TRUE(next(itr_ck.begin()) == itr_ck.end());
        EXPECT_TRUE(prior(itr_ck.end()) == itr_ck.begin());
        EXPECT_TRUE(*prior(itr_ck.end()) == std::string("c:"));

        itr_ck = Path("c:/");
        EXPECT_TRUE(*itr_ck.begin() == std::string("c:"));
        EXPECT_TRUE(*next(itr_ck.begin()) == std::string("/"));
        EXPECT_TRUE(next(next(itr_ck.begin())) == itr_ck.end());
        EXPECT_TRUE(prior(prior(itr_ck.end())) == itr_ck.begin());
        EXPECT_TRUE(*prior(itr_ck.end()) == std::string("/"));
        EXPECT_TRUE(*prior(prior(itr_ck.end())) == std::string("c:"));

        itr_ck = Path("c:foo");
        EXPECT_TRUE(*itr_ck.begin() == std::string("c:"));
        EXPECT_TRUE(*next(itr_ck.begin()) == std::string("foo"));
        EXPECT_TRUE(next(next(itr_ck.begin())) == itr_ck.end());
        EXPECT_TRUE(prior(prior(itr_ck.end())) == itr_ck.begin());
        EXPECT_TRUE(*prior(itr_ck.end()) == std::string("foo"));
        EXPECT_TRUE(*prior(prior(itr_ck.end())) == std::string("c:"));

        itr_ck = Path("c:/foo");
        EXPECT_TRUE(*itr_ck.begin() == std::string("c:"));
        EXPECT_TRUE(*next(itr_ck.begin()) == std::string("/"));
        EXPECT_TRUE(*next(next(itr_ck.begin())) == std::string("foo"));
        EXPECT_TRUE(next(next(next(itr_ck.begin()))) == itr_ck.end());
        EXPECT_TRUE(prior(prior(prior(itr_ck.end()))) == itr_ck.begin());
        EXPECT_TRUE(*prior(itr_ck.end()) == std::string("foo"));
        EXPECT_TRUE(*prior(prior(itr_ck.end())) == std::string("/"));
        EXPECT_TRUE(*prior(prior(prior(itr_ck.end()))) == std::string("c:"));

        itr_ck = Path("//net");
        EXPECT_TRUE(*itr_ck.begin() == std::string("//net"));
        EXPECT_TRUE(next(itr_ck.begin()) == itr_ck.end());
        EXPECT_TRUE(prior(itr_ck.end()) == itr_ck.begin());
        EXPECT_TRUE(*prior(itr_ck.end()) == std::string("//net"));

        itr_ck = Path("//net/");
        EXPECT_EQ(itr_ck.begin()->string(), "//net");
        EXPECT_EQ(next(itr_ck.begin())->string(), "/");
        EXPECT_TRUE(next(next(itr_ck.begin())) == itr_ck.end());
        EXPECT_TRUE(prior(prior(itr_ck.end())) == itr_ck.begin());
        EXPECT_EQ(prior(itr_ck.end())->string(), "/");
        EXPECT_EQ(prior(prior(itr_ck.end()))->string(), "//net");

        itr_ck = Path("//net/foo");
        EXPECT_TRUE(*itr_ck.begin() == std::string("//net"));
        EXPECT_TRUE(*next(itr_ck.begin()) == std::string("/"));
        EXPECT_TRUE(*next(next(itr_ck.begin())) == std::string("foo"));
        EXPECT_TRUE(next(next(next(itr_ck.begin()))) == itr_ck.end());
        EXPECT_TRUE(prior(prior(prior(itr_ck.end()))) == itr_ck.begin());
        EXPECT_TRUE(*prior(itr_ck.end()) == std::string("foo"));
        EXPECT_TRUE(*prior(prior(itr_ck.end())) == std::string("/"));
        EXPECT_TRUE(*prior(prior(prior(itr_ck.end()))) == std::string("//net"));

        itr_ck = Path("prn:");
        EXPECT_TRUE(*itr_ck.begin() == std::string("prn:"));
        EXPECT_TRUE(next(itr_ck.begin()) == itr_ck.end());
        EXPECT_TRUE(prior(itr_ck.end()) == itr_ck.begin());
        EXPECT_TRUE(*prior(itr_ck.end()) == std::string("prn:"));
    }
#else
    {
        itr_ck = "///";
        itr = itr_ck.begin();
        EXPECT_EQ(itr->string(),  "/");
        EXPECT_TRUE(++itr == itr_ck.end());
    }
#endif
}

TEST_F(PathTest, reverse_iterators) {
    Path p1;
    EXPECT_TRUE(p1.rbegin() == p1.rend());

    Path p2("/");
    EXPECT_TRUE(p2.rbegin() != p2.rend());
    EXPECT_TRUE(*p2.rbegin() == "/");
    EXPECT_TRUE(++p2.rbegin() == p2.rend());

    Path p3("foo/bar/baz");

    Path::reverse_iterator it(p3.rbegin());
    EXPECT_TRUE(p3.rbegin() != p3.rend());
    EXPECT_TRUE(*it == "baz");
    EXPECT_TRUE(*++it == "bar");
    EXPECT_TRUE(*++it == "foo");
    EXPECT_TRUE(*--it == "bar");
    EXPECT_TRUE(*--it == "baz");
    EXPECT_TRUE(*++it == "bar");
    EXPECT_TRUE(*++it == "foo");
    EXPECT_TRUE(++it == p3.rend());
}

TEST_F(PathTest, modifiers) {
    EXPECT_TRUE(Path("").remove_filename() == "");
    EXPECT_TRUE(Path("foo").remove_filename() == "");
    EXPECT_TRUE(Path("/foo").remove_filename() == "/");
    EXPECT_TRUE(Path("foo/bar").remove_filename() == "foo");
    EXPECT_EQ(Path("foo/bar/").remove_filename(), Path("foo/bar"));
    EXPECT_EQ(Path(".").remove_filename(), Path(""));
    EXPECT_EQ(Path("./.").remove_filename(), Path("."));
    EXPECT_EQ(Path("/.").remove_filename(), Path("/"));
    EXPECT_EQ(Path("..").remove_filename(), Path(""));
    EXPECT_EQ(Path("../..").remove_filename(), Path(".."));
    EXPECT_EQ(Path("/..").remove_filename(), Path("/"));
}

TEST_F(PathTest, decompositions) {
    EXPECT_TRUE(Path("").root_name().string() == "");
    EXPECT_TRUE(Path("foo").root_name().string() == "");
    EXPECT_TRUE(Path("/").root_name().string() == "");
    EXPECT_TRUE(Path("/foo").root_name().string() == "");
    EXPECT_TRUE(Path("//netname").root_name().string() == "//netname");
    EXPECT_TRUE(Path("//netname/foo").root_name().string() == "//netname");

    EXPECT_TRUE(Path("").root_directory().string() == "");
    EXPECT_TRUE(Path("foo").root_directory().string() == "");
    EXPECT_TRUE(Path("/").root_directory().string() == "/");
    EXPECT_TRUE(Path("/foo").root_directory().string() == "/");
    EXPECT_TRUE(Path("//netname").root_directory().string() == "");
    EXPECT_TRUE(Path("//netname/foo").root_directory().string() == "/");

    EXPECT_TRUE(Path("").root_path().string() == "");
    EXPECT_TRUE(Path("/").root_path().string() == "/");
    EXPECT_TRUE(Path("/foo").root_path().string() == "/");
    EXPECT_TRUE(Path("//netname").root_path().string() == "//netname");
    EXPECT_TRUE(Path("//netname/foo").root_path().string() == "//netname/");

    // TODO: fixme
#   ifdef BOOST_WINDOWS_API
    EXPECT_TRUE(Path("c:/foo").root_path().string() == "c:/");
#   endif

    EXPECT_TRUE(Path("").relative_path().string() == "");
    EXPECT_TRUE(Path("/").relative_path().string() == "");
    EXPECT_TRUE(Path("/foo").relative_path().string() == "foo");

    EXPECT_TRUE(Path("").parent_path().string() == "");
    EXPECT_TRUE(Path("/").parent_path().string() == "");
    EXPECT_TRUE(Path("/foo").parent_path().string() == "/");
    EXPECT_TRUE(Path("/foo/bar").parent_path().string() == "/foo");

    EXPECT_TRUE(Path("/foo/bar/baz.zoo").filename().string() == "baz.zoo");

    EXPECT_TRUE(Path("/foo/bar/baz.zoo").stem().string() == "baz");
    EXPECT_TRUE(Path("/foo/bar.woo/baz").stem().string() == "baz");

    EXPECT_TRUE(Path("foo.bar.baz.tar.bz2").extension().string() == ".bz2");
    EXPECT_TRUE(Path("/foo/bar/baz.zoo").extension().string() == ".zoo");
    EXPECT_TRUE(Path("/foo/bar.woo/baz").extension().string() == "");
}


TEST_F(PathTest, queries) {
    Path p1("");
    Path p2("//netname/foo.doo");

    EXPECT_TRUE(p1.empty());
    EXPECT_TRUE(!p1.has_root_path());
    EXPECT_TRUE(!p1.has_root_name());
    EXPECT_TRUE(!p1.has_root_directory());
    EXPECT_TRUE(!p1.has_relative_path());
    EXPECT_TRUE(!p1.has_parent_path());
    EXPECT_TRUE(!p1.has_filename());
    EXPECT_TRUE(!p1.has_stem());
    EXPECT_TRUE(!p1.has_extension());
    EXPECT_TRUE(!p1.is_absolute());
    EXPECT_TRUE(p1.is_relative());

    EXPECT_TRUE(!p2.empty());
    EXPECT_TRUE(p2.has_root_path());
    EXPECT_TRUE(p2.has_root_name());
    EXPECT_TRUE(p2.has_root_directory());
    EXPECT_TRUE(p2.has_relative_path());
    EXPECT_TRUE(p2.has_parent_path());
    EXPECT_TRUE(p2.has_filename());
    EXPECT_TRUE(p2.has_stem());
    EXPECT_TRUE(p2.has_extension());
    EXPECT_TRUE(p2.is_absolute());
    EXPECT_TRUE(!p2.is_relative());
}

TEST_F(PathTest, imbue_locale) {
    //  weak test case for before/after states since we don't know what characters the
    //  default locale accepts.
    Path before("abc");

    //  So that tests are run with known encoding, use Boost UTF-8 codecvt
    //  \u2722 and \xE2\x9C\xA2 are UTF-16 and UTF-8 FOUR TEARDROP-SPOKED ASTERISK

    std::locale global_loc = std::locale();
    // TODO: fixme boost::filesystem::detail::utf8_codecvt_facet
    std::locale loc(global_loc, new io::detail::utf8_codecvt_facet);
    std::locale old_loc = Path::imbue(loc);

    Path p2("\xE2\x9C\xA2");
    EXPECT_TRUE(p2.wstring().size() == 1);
    EXPECT_TRUE(p2.wstring()[0] == 0x2722);

    Path::imbue(old_loc);

    Path after("abc");
    EXPECT_TRUE(before == after);
}

namespace {

#include <locale>
#include <cwchar>  // for mbstate_t

  //------------------------------------------------------------------------------------//
  //                                                                                    //
  //                              class test_codecvt                                    //
  //                                                                                    //
  //  Warning: partial implementation; even do_in and do_out only partially meet the    //
  //  standard library specifications as the "to" buffer must hold the entire result.   //
  //                                                                                    //
  //  The value of a wide character is the value of the corresponding narrow character  //
  //  plus 1. This ensures that compares against expected values will fail if the       //
  //  code conversion did not occur as expected.                                        //
  //                                                                                    //
  //------------------------------------------------------------------------------------//

class test_codecvt : public std::codecvt< wchar_t, char, std::mbstate_t> {
public:
    explicit test_codecvt()
        : std::codecvt<wchar_t, char, std::mbstate_t>() {}

protected:
    virtual bool do_always_noconv() const throw() { return false; }
    virtual int do_encoding() const throw() { return 0; }

    virtual std::codecvt_base::result do_in(std::mbstate_t&,
            const char* from, const char* from_end, const char*& from_next,
            wchar_t* to, wchar_t* to_end, wchar_t*& to_next) const {
        for (; from != from_end && to != to_end; ++from, ++to)
            *to = wchar_t(*from + 1);

        if (to == to_end)
            return error;

        *to = L'\0';
        from_next = from;
        to_next = to;
        return ok;
    }

virtual std::codecvt_base::result do_out(std::mbstate_t&,
            const wchar_t* from, const wchar_t* from_end, const wchar_t*& from_next,
            char* to, char* to_end, char*& to_next) const {
        for (; from != from_end && to != to_end; ++from, ++to)
            *to = static_cast<char>(*from - 1);

        if (to == to_end)
            return error;

        *to = '\0';
        from_next = from;
        to_next = to;
        return ok;
    }

    virtual std::codecvt_base::result do_unshift(std::mbstate_t&,
        char* /*from*/, char* /*to*/, char* & /*next*/) const  { return ok; }

    virtual int do_length(std::mbstate_t&,
        const char* /*from*/, const char* /*from_end*/, std::size_t /*max*/) const  { return 0; }

    virtual int do_max_length() const throw () { return 0; }
};

// TODO: fixme
# ifdef BOOST_WINDOWS_API
  void check_native(const Path& p,
    const std::string&, const std::wstring& expected)
# else
  void check_native(const Path& p,
    const std::string& expected, const std::wstring&)
# endif
{
    EXPECT_EQ(p.native(), expected);
}

} // namespace

TEST_F(PathTest, codecvt_argument) {
    const char * c1 = "a1";
    const std::string s1(c1);
    const std::wstring ws1(L"b2");  // off-by-one mimics test_codecvt
    const std::string s2("y8");
    const std::wstring ws2(L"z9");

    test_codecvt cvt;  // produces off-by-one values that will always differ from
                       // the system's default locale codecvt facet

    //  constructors
    Path p(c1, cvt);
    check_native(p, s1, ws1);

    Path p1(s1.begin(), s1.end(), cvt);
    check_native(p1, s1, ws1);

    Path p2(ws2, cvt);
    check_native(p2, s2, ws2);

    Path p3(ws2.begin(), ws2.end(), cvt);
    check_native(p3, s2, ws2);

    // Path p2(p1, cvt);  // fails to compile, and that is OK

    //  assigns
    p1.clear();
    p1.assign(s1,cvt);
    check_native(p1, s1, ws1);
    p1.clear();
    p1.assign(s1.begin(), s1.end(), cvt);
    check_native(p1, s1, ws1);
    // p1.assign(p, cvt);  // fails to compile, and that is OK

    //  appends
    p1.clear();
    p1.append(s1,cvt);
    check_native(p1, s1, ws1);
    p1.clear();
    p1.append(s1.begin(), s1.end(), cvt);
    check_native(p1, s1, ws1);
    // p1.append(p, cvt);  // fails to compile, and that is OK

    //  native observers
    EXPECT_TRUE(p.string<std::string>(cvt) == s1);
    EXPECT_TRUE(p.string(cvt) == s1);
    EXPECT_TRUE(p.string<std::wstring>(cvt) == ws1);
    EXPECT_TRUE(p.wstring(cvt) == ws1);

    //  generic observers
    EXPECT_TRUE(p.generic_string<std::string>(cvt) == s1);
    EXPECT_TRUE(p.generic_string(cvt) == s1);
    EXPECT_TRUE(p.generic_string<std::wstring>(cvt) == ws1);
    EXPECT_TRUE(p.generic_wstring(cvt) == ws1);
}

TEST_F(PathTest, overloads) {
    std::string sto("hello");
    const char a[] = "goodbye";
    Path p1(sto);
    Path p2(sto.c_str());
    Path p3(a);
    Path p4("foo");

    EXPECT_FALSE(p1.empty());
    EXPECT_FALSE(p2.empty());
    EXPECT_FALSE(p3.empty());
    EXPECT_FALSE(p4.empty());

    std::wstring wsto(L"hello");
    const wchar_t wa[] = L"goodbye";
    Path wp1(wsto);
    Path wp2(wsto.c_str());
    Path wp3(wa);
    Path wp4(L"foo");

    EXPECT_FALSE(wp1.empty());
    EXPECT_FALSE(wp2.empty());
    EXPECT_FALSE(wp3.empty());
    EXPECT_FALSE(wp4.empty());
}

TEST_F(PathTest, non_member) {
    // test non-member functions, particularly operator overloads

    Path e, e2;
    std::string es, es2;
    char ecs[] = "";
    char ecs2[] = "";

    char acs[] = "a";
    std::string as(acs);
    Path a(as);

    char acs2[] = "a";
    std::string as2(acs2);
    Path a2(as2);

    char bcs[] = "b";
    std::string bs(bcs);
    Path b(bs);

    // swap
    a.swap(b);
    EXPECT_TRUE(a.string() == "b");
    EXPECT_TRUE(b.string() == "a");

    b.swap(a);
    EXPECT_TRUE(a.string() == "a");
    EXPECT_TRUE(b.string() == "b");

    // probe operator /
    EXPECT_EQ(Path("") / ".", ".");
    EXPECT_EQ(Path("") / "..", "..");

// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
        EXPECT_TRUE(Path("foo\\bar") == "foo/bar");
        EXPECT_TRUE((b / a).native() == Path("b\\a").native());
        EXPECT_TRUE((bs / a).native() == Path("b\\a").native());
        EXPECT_TRUE((bcs / a).native() == Path("b\\a").native());
        EXPECT_TRUE((b / as).native() == Path("b\\a").native());
        EXPECT_TRUE((b / acs).native() == Path("b\\a").native());
        EXPECT_EQ(Path("a") / "b", "a\\b");
        EXPECT_EQ(Path("..") / "", "..");
        EXPECT_EQ(Path("foo") / Path("bar"), "foo\\bar"); // Path arg
        EXPECT_EQ(Path("foo") / "bar", "foo\\bar");       // const char* arg
        EXPECT_EQ(Path("foo") / Path("woo/bar").filename(), "foo\\bar"); // const std::string & arg
        EXPECT_EQ("foo" / Path("bar"), "foo\\bar");
        EXPECT_EQ(Path("..") / ".." , "..\\..");
        EXPECT_EQ(Path("/") / ".." , "/..");
        EXPECT_EQ(Path("/..") / ".." , "/..\\..");
        EXPECT_EQ(Path("..") / "foo" , "..\\foo");
        EXPECT_EQ(Path("foo") / ".." , "foo\\..");
        EXPECT_EQ(Path("..") / "f" , "..\\f");
        EXPECT_EQ(Path("/..") / "f" , "/..\\f");
        EXPECT_EQ(Path("f") / ".." , "f\\..");
        EXPECT_EQ(Path("foo") / ".." / ".." , "foo\\..\\..");
        EXPECT_EQ(Path("foo") / ".." / ".." / ".." , "foo\\..\\..\\..");
        EXPECT_EQ(Path("f") / ".." / "b" , "f\\..\\b");
        EXPECT_EQ(Path("foo") / ".." / "bar" , "foo\\..\\bar");
        EXPECT_EQ(Path("foo") / "bar" / ".." , "foo\\bar\\..");
        EXPECT_EQ(Path("foo") / "bar" / ".." / "..", "foo\\bar\\..\\..");
        EXPECT_EQ(Path("foo") / "bar" / ".." / "blah", "foo\\bar\\..\\blah");
        EXPECT_EQ(Path("f") / "b" / ".." , "f\\b\\..");
        EXPECT_EQ(Path("f") / "b" / ".." / "a", "f\\b\\..\\a");
        EXPECT_EQ(Path("foo") / "bar" / "blah" / ".." / "..", "foo\\bar\\blah\\..\\..");
        EXPECT_EQ(Path("foo") / "bar" / "blah" / ".." / ".." / "bletch", "foo\\bar\\blah\\..\\..\\bletch");

        EXPECT_EQ(Path(".") / "foo", ".\\foo");
        EXPECT_EQ(Path(".") / "..", ".\\..");
        EXPECT_EQ(Path("foo") / ".", "foo\\.");
        EXPECT_EQ(Path("..") / ".", "..\\.");
        EXPECT_EQ(Path(".") / ".", ".\\.");
        EXPECT_EQ(Path(".") / "." / ".", ".\\.\\.");
        EXPECT_EQ(Path(".") / "foo" / ".", ".\\foo\\.");
        EXPECT_EQ(Path("foo") / "." / "bar", "foo\\.\\bar");
        EXPECT_EQ(Path("foo") / "." / ".", "foo\\.\\.");
        EXPECT_EQ(Path("foo") / "." / "..", "foo\\.\\..");
        EXPECT_EQ(Path(".") / "." / "..", ".\\.\\..");
        EXPECT_EQ(Path(".") / ".." / ".", ".\\..\\.");
        EXPECT_EQ(Path("..") / "." / ".", "..\\.\\.");
#else
        EXPECT_EQ(b / a, "b/a");
        EXPECT_EQ(bs / a, "b/a");
        EXPECT_EQ(bcs / a, "b/a");
        EXPECT_EQ(b / as, "b/a");
        EXPECT_EQ(b / acs, "b/a");
        EXPECT_EQ(Path("a") / "b", "a/b");
        EXPECT_EQ(Path("..") / "", "..");
        EXPECT_EQ(Path("") / "..", "..");
        EXPECT_EQ(Path("foo") / Path("bar"), "foo/bar"); // Path arg
        EXPECT_EQ(Path("foo") / "bar", "foo/bar");       // const char* arg
        EXPECT_EQ(Path("foo") / Path("woo/bar").filename(), "foo/bar"); // const std::string & arg
        EXPECT_EQ("foo" / Path("bar"), "foo/bar");
        EXPECT_EQ(Path("..") / ".." , "../..");
        EXPECT_EQ(Path("/") / ".." , "/..");
        EXPECT_EQ(Path("/..") / ".." , "/../..");
        EXPECT_EQ(Path("..") / "foo" , "../foo");
        EXPECT_EQ(Path("foo") / ".." , "foo/..");
        EXPECT_EQ(Path("..") / "f" , "../f");
        EXPECT_EQ(Path("/..") / "f" , "/../f");
        EXPECT_EQ(Path("f") / ".." , "f/..");
        EXPECT_EQ(Path("foo") / ".." / ".." , "foo/../..");
        EXPECT_EQ(Path("foo") / ".." / ".." / ".." , "foo/../../..");
        EXPECT_EQ(Path("f") / ".." / "b" , "f/../b");
        EXPECT_EQ(Path("foo") / ".." / "bar" , "foo/../bar");
        EXPECT_EQ(Path("foo") / "bar" / ".." , "foo/bar/..");
        EXPECT_EQ(Path("foo") / "bar" / ".." / "..", "foo/bar/../..");
        EXPECT_EQ(Path("foo") / "bar" / ".." / "blah", "foo/bar/../blah");
        EXPECT_EQ(Path("f") / "b" / ".." , "f/b/..");
        EXPECT_EQ(Path("f") / "b" / ".." / "a", "f/b/../a");
        EXPECT_EQ(Path("foo") / "bar" / "blah" / ".." / "..", "foo/bar/blah/../..");
        EXPECT_EQ(Path("foo") / "bar" / "blah" / ".." / ".." / "bletch", "foo/bar/blah/../../bletch");

        EXPECT_EQ(Path(".") / "foo", "./foo");
        EXPECT_EQ(Path(".") / "..", "./..");
        EXPECT_EQ(Path("foo") / ".", "foo/.");
        EXPECT_EQ(Path("..") / ".", "../.");
        EXPECT_EQ(Path(".") / ".", "./.");
        EXPECT_EQ(Path(".") / "." / ".", "././.");
        EXPECT_EQ(Path(".") / "foo" / ".", "./foo/.");
        EXPECT_EQ(Path("foo") / "." / "bar", "foo/./bar");
        EXPECT_EQ(Path("foo") / "." / ".", "foo/./.");
        EXPECT_EQ(Path("foo") / "." / "..", "foo/./..");
        EXPECT_EQ(Path(".") / "." / "..", "././..");
        EXPECT_EQ(Path(".") / ".." / ".", "./../.");
        EXPECT_EQ(Path("..") / "." / ".", ".././.");
#endif

    // probe operator <
    EXPECT_TRUE(!(e < e2));
    EXPECT_TRUE(!(es < e2));
    EXPECT_TRUE(!(ecs < e2));
    EXPECT_TRUE(!(e < es2));
    EXPECT_TRUE(!(e < ecs2));

    EXPECT_TRUE(e < a);
    EXPECT_TRUE(es < a);
    EXPECT_TRUE(ecs < a);
    EXPECT_TRUE(e < as);
    EXPECT_TRUE(e < acs);

    EXPECT_TRUE(a < b);
    EXPECT_TRUE(as < b);
    EXPECT_TRUE(acs < b);
    EXPECT_TRUE(a < bs);
    EXPECT_TRUE(a < bcs);

    EXPECT_TRUE(!(a < a2));
    EXPECT_TRUE(!(as < a2));
    EXPECT_TRUE(!(acs < a2));
    EXPECT_TRUE(!(a < as2));
    EXPECT_TRUE(!(a < acs2));

    // make sure basic_Path overloads don't conflict with std::string overloads

    EXPECT_TRUE(!(as < as));
    EXPECT_TRUE(!(as < acs));
    EXPECT_TRUE(!(acs < as));

    // character set reality check before lexicographical tests
    EXPECT_TRUE(std::string("a.b") < std::string("a/b"));
    // verify compare is actually lexicographical
    EXPECT_TRUE(Path("a/b") < Path("a.b"));
    EXPECT_TRUE(Path("a/b") == Path("a///b"));
    EXPECT_TRUE(Path("a/b/") == Path("a/b/."));
    EXPECT_TRUE(Path("a/b") != Path("a/b/"));

    // make sure the derivative operators also work

    EXPECT_TRUE(b > a);
    EXPECT_TRUE(b > as);
    EXPECT_TRUE(b > acs);
    EXPECT_TRUE(bs > a);
    EXPECT_TRUE(bcs > a);

    EXPECT_TRUE(!(a2 > a));
    EXPECT_TRUE(!(a2 > as));
    EXPECT_TRUE(!(a2 > acs));
    EXPECT_TRUE(!(as2 > a));
    EXPECT_TRUE(!(acs2 > a));

    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(as <= b);
    EXPECT_TRUE(acs <= b);
    EXPECT_TRUE(a <= bs);
    EXPECT_TRUE(a <= bcs);

    EXPECT_TRUE(a <= a2);
    EXPECT_TRUE(as <= a2);
    EXPECT_TRUE(acs <= a2);
    EXPECT_TRUE(a <= as2);
    EXPECT_TRUE(a <= acs2);

    EXPECT_TRUE(b >= a);
    EXPECT_TRUE(bs >= a);
    EXPECT_TRUE(bcs >= a);
    EXPECT_TRUE(b >= as);
    EXPECT_TRUE(b >= acs);

    EXPECT_TRUE(a2 >= a);
    EXPECT_TRUE(as2 >= a);
    EXPECT_TRUE(acs2 >= a);
    EXPECT_TRUE(a2 >= as);
    EXPECT_TRUE(a2 >= acs);

    //  operator == and != are implemented separately, so test separately

    Path p101("fe/fi/fo/fum");
    Path p102(p101);
    Path p103("fe/fi/fo/fumm");
    EXPECT_TRUE(p101.string() != p103.string());

    // check each overload
    EXPECT_TRUE(p101 != p103);
    EXPECT_TRUE(p101 != p103.string());
    EXPECT_TRUE(p101 != p103.string().c_str());
    EXPECT_TRUE(p101.string() != p103);
    EXPECT_TRUE(p101.string().c_str() != p103);

    p103 = p102;
    EXPECT_TRUE(p101.string() == p103.string());

    // check each overload
    EXPECT_TRUE(p101 == p103);
    EXPECT_TRUE(p101 == p103.string());
    EXPECT_TRUE(p101 == p103.string().c_str());
    EXPECT_TRUE(p101.string() == p103);
    EXPECT_TRUE(p101.string().c_str() == p103);

// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
        Path p10 ("c:\\file");
        Path p11 ("c:/file");
        // check each overload
        EXPECT_TRUE(p10.generic_string() == p11.generic_string());
        EXPECT_TRUE(p10 == p11);
        EXPECT_TRUE(p10 == p11.string());
        EXPECT_TRUE(p10 == p11.string().c_str());
        EXPECT_TRUE(p10.string() == p11);
        EXPECT_TRUE(p10.string().c_str() == p11);
        EXPECT_TRUE(p10 == L"c:\\file");
        EXPECT_TRUE(p10 == L"c:/file");
        EXPECT_TRUE(p11 == L"c:\\file");
        EXPECT_TRUE(p11 == L"c:/file");
        EXPECT_TRUE(L"c:\\file" == p10);
        EXPECT_TRUE(L"c:/file" == p10);
        EXPECT_TRUE(L"c:\\file" == p11);
        EXPECT_TRUE(L"c:/file" == p11);

        EXPECT_TRUE(!(p10.generic_string() != p11.generic_string()));
        EXPECT_TRUE(!(p10 != p11));
        EXPECT_TRUE(!(p10 != p11.string()));
        EXPECT_TRUE(!(p10 != p11.string().c_str()));
        EXPECT_TRUE(!(p10.string() != p11));
        EXPECT_TRUE(!(p10.string().c_str() != p11));
        EXPECT_TRUE(!(p10 != L"c:\\file"));
        EXPECT_TRUE(!(p10 != L"c:/file"));
        EXPECT_TRUE(!(p11 != L"c:\\file"));
        EXPECT_TRUE(!(p11 != L"c:/file"));
        EXPECT_TRUE(!(L"c:\\file" != p10));
        EXPECT_TRUE(!(L"c:/file" != p10));
        EXPECT_TRUE(!(L"c:\\file" != p11));
        EXPECT_TRUE(!(L"c:/file" != p11));

        EXPECT_TRUE(!(p10.string() < p11.string()));
        EXPECT_TRUE(!(p10 < p11));
        EXPECT_TRUE(!(p10 < p11.string()));
        EXPECT_TRUE(!(p10 < p11.string().c_str()));
        EXPECT_TRUE(!(p10.string() < p11));
        EXPECT_TRUE(!(p10.string().c_str() < p11));
        EXPECT_TRUE(!(p10 < L"c:\\file"));
        EXPECT_TRUE(!(p10 < L"c:/file"));
        EXPECT_TRUE(!(p11 < L"c:\\file"));
        EXPECT_TRUE(!(p11 < L"c:/file"));
        EXPECT_TRUE(!(L"c:\\file" < p10));
        EXPECT_TRUE(!(L"c:/file" < p10));
        EXPECT_TRUE(!(L"c:\\file" < p11));
        EXPECT_TRUE(!(L"c:/file" < p11));

        EXPECT_TRUE(!(p10.generic_string() > p11.generic_string()));
        EXPECT_TRUE(!(p10 > p11));
        EXPECT_TRUE(!(p10 > p11.string()));
        EXPECT_TRUE(!(p10 > p11.string().c_str()));
        EXPECT_TRUE(!(p10.string() > p11));
        EXPECT_TRUE(!(p10.string().c_str() > p11));
        EXPECT_TRUE(!(p10 > L"c:\\file"));
        EXPECT_TRUE(!(p10 > L"c:/file"));
        EXPECT_TRUE(!(p11 > L"c:\\file"));
        EXPECT_TRUE(!(p11 > L"c:/file"));
        EXPECT_TRUE(!(L"c:\\file" > p10));
        EXPECT_TRUE(!(L"c:/file" > p10));
        EXPECT_TRUE(!(L"c:\\file" > p11));
        EXPECT_TRUE(!(L"c:/file" > p11));
#endif
}

TEST_F(PathTest, query_and_decomposition) {
    // these are the examples given in reference docs, so check they work
    EXPECT_TRUE(Path("/foo/bar.txt").parent_path() == "/foo");
    EXPECT_TRUE(Path("/foo/bar").parent_path() == "/foo");
    EXPECT_TRUE(Path("/foo/bar/").parent_path() == "/foo/bar");
    EXPECT_TRUE(Path("/").parent_path() == "");
    EXPECT_TRUE(Path(".").parent_path() == "");
    EXPECT_TRUE(Path("..").parent_path() == "");
    EXPECT_TRUE(Path("/foo/bar.txt").filename() == "bar.txt");
    EXPECT_TRUE(Path("/foo/bar").filename() == "bar");
    EXPECT_TRUE(Path("/foo/bar/").filename() == ".");
    EXPECT_TRUE(Path("/").filename() == "/");
    EXPECT_TRUE(Path(".").filename() == ".");
    EXPECT_TRUE(Path("..").filename() == "..");

    // stem() tests not otherwise covered
    EXPECT_TRUE(Path(".").stem() == ".");
    EXPECT_TRUE(Path("..").stem() == "..");
    EXPECT_TRUE(Path(".a").stem() == "");
    EXPECT_TRUE(Path("b").stem() == "b");
    EXPECT_TRUE(Path("a/b.txt").stem() == "b");
    EXPECT_TRUE(Path("a/b.").stem() == "b");
    EXPECT_TRUE(Path("a.b.c").stem() == "a.b");
    EXPECT_TRUE(Path("a.b.c.").stem() == "a.b.c");

    // extension() tests not otherwise covered
    EXPECT_TRUE(Path(".").extension() == "");
    EXPECT_TRUE(Path("..").extension() == "");
    EXPECT_TRUE(Path(".a").extension() == ".a");
    EXPECT_TRUE(Path("a/b").extension() == "");
    EXPECT_TRUE(Path("a.b/c").extension() == "");
    EXPECT_TRUE(Path("a/b.txt").extension() == ".txt");
    EXPECT_TRUE(Path("a/b.").extension() == ".");
    EXPECT_TRUE(Path("a.b.c").extension() == ".c");
    EXPECT_TRUE(Path("a.b.c.").extension() == ".");
    EXPECT_TRUE(Path("a/").extension() == "");

    // main q & d test sequence
    Path p;
    Path q;

    p = q = "";
    EXPECT_TRUE(p.relative_path().string() == "");
    EXPECT_TRUE(p.parent_path().string() == "");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_TRUE(p.filename() == "");
    EXPECT_TRUE(p.stem() == "");
    EXPECT_TRUE(p.extension() == "");
    EXPECT_TRUE(p.root_name() == "");
    EXPECT_TRUE(p.root_directory() == "");
    EXPECT_TRUE(p.root_path().string() == "");
    EXPECT_TRUE(!p.has_root_path());
    EXPECT_TRUE(!p.has_root_name());
    EXPECT_TRUE(!p.has_root_directory());
    EXPECT_TRUE(!p.has_relative_path());
    EXPECT_TRUE(!p.has_filename());
    EXPECT_TRUE(!p.has_stem());
    EXPECT_TRUE(!p.has_extension());
    EXPECT_TRUE(!p.has_parent_path());
    EXPECT_TRUE(!p.is_absolute());

    p = q = "/";
    EXPECT_TRUE(p.relative_path().string() == "");
    EXPECT_TRUE(p.parent_path().string() == "");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_TRUE(p.filename() == "/");
    EXPECT_TRUE(p.stem() == "/");
    EXPECT_TRUE(p.extension() == "");
    EXPECT_TRUE(p.root_name() == "");
    EXPECT_TRUE(p.root_directory() == "/");
    EXPECT_TRUE(p.root_path().string() == "/");
    EXPECT_TRUE(p.has_root_path());
    EXPECT_TRUE(!p.has_root_name());
    EXPECT_TRUE(p.has_root_directory());
    EXPECT_TRUE(!p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_stem());
    EXPECT_TRUE(!p.has_extension());
    EXPECT_TRUE(!p.has_parent_path());
// TODO: fixme
#ifndef BUILD_FOR_WINDOWS
      EXPECT_TRUE(p.is_absolute());
#else
      EXPECT_TRUE(!p.is_absolute());
#endif

    p = q = "//";
    EXPECT_EQ(p.relative_path().string(), "");
    EXPECT_EQ(p.parent_path().string(), "");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_EQ(p.filename(), "//");
    EXPECT_EQ(p.stem(), "//");
    EXPECT_EQ(p.extension(), "");
    EXPECT_EQ(p.root_name(), "//");
    EXPECT_EQ(p.root_directory(), "");
    EXPECT_EQ(p.root_path().string(), "//");
    EXPECT_TRUE(p.has_root_path());
    EXPECT_TRUE(p.has_root_name());
    EXPECT_TRUE(!p.has_root_directory());
    EXPECT_TRUE(!p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_stem());
    EXPECT_TRUE(!p.has_extension());
    EXPECT_TRUE(!p.has_parent_path());
    EXPECT_TRUE(!p.is_absolute());

    p = q = "///";
    EXPECT_EQ(p.relative_path().string(), "");
    EXPECT_EQ(p.parent_path().string(), "");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_EQ(p.filename(), "/");
    EXPECT_EQ(p.stem(), "/");
    EXPECT_EQ(p.extension(), "");
    EXPECT_EQ(p.root_name(), "");
    EXPECT_EQ(p.root_directory(), "/");
    EXPECT_EQ(p.root_path().string(), "/");
    EXPECT_TRUE(p.has_root_path());
    EXPECT_TRUE(!p.has_root_name());
    EXPECT_TRUE(p.has_root_directory());
    EXPECT_TRUE(!p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_stem());
    EXPECT_TRUE(!p.has_extension());
    EXPECT_TRUE(!p.has_parent_path());
// TODO: fixme
#ifndef BUILD_FOR_WINDOWS
      EXPECT_TRUE(p.is_absolute());
#else
      EXPECT_TRUE(!p.is_absolute());
#endif

    p = q = ".";
    EXPECT_TRUE(p.relative_path().string() == ".");
    EXPECT_TRUE(p.parent_path().string() == "");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_TRUE(p.filename() == ".");
    EXPECT_TRUE(p.stem() == ".");
    EXPECT_TRUE(p.extension() == "");
    EXPECT_TRUE(p.root_name() == "");
    EXPECT_TRUE(p.root_directory() == "");
    EXPECT_TRUE(p.root_path().string() == "");
    EXPECT_TRUE(!p.has_root_path());
    EXPECT_TRUE(!p.has_root_name());
    EXPECT_TRUE(!p.has_root_directory());
    EXPECT_TRUE(p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_stem());
    EXPECT_TRUE(!p.has_extension());
    EXPECT_TRUE(!p.has_parent_path());
    EXPECT_TRUE(!p.is_absolute());

    p = q = "..";
    EXPECT_TRUE(p.relative_path().string() == "..");
    EXPECT_TRUE(p.parent_path().string() == "");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_TRUE(p.filename() == "..");
    EXPECT_TRUE(p.stem() == "..");
    EXPECT_TRUE(p.extension() == "");
    EXPECT_TRUE(p.root_name() == "");
    EXPECT_TRUE(p.root_directory() == "");
    EXPECT_TRUE(p.root_path().string() == "");
    EXPECT_TRUE(!p.has_root_path());
    EXPECT_TRUE(!p.has_root_name());
    EXPECT_TRUE(!p.has_root_directory());
    EXPECT_TRUE(p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_stem());
    EXPECT_TRUE(!p.has_extension());
    EXPECT_TRUE(!p.has_parent_path());
    EXPECT_TRUE(!p.is_absolute());

    p = q = "foo";
    EXPECT_TRUE(p.relative_path().string() == "foo");
    EXPECT_TRUE(p.parent_path().string() == "");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_TRUE(p.filename() == "foo");
    EXPECT_TRUE(p.stem() == "foo");
    EXPECT_TRUE(p.extension() == "");
    EXPECT_TRUE(p.root_name() == "");
    EXPECT_TRUE(p.root_directory() == "");
    EXPECT_TRUE(p.root_path().string() == "");
    EXPECT_TRUE(!p.has_root_path());
    EXPECT_TRUE(!p.has_root_name());
    EXPECT_TRUE(!p.has_root_directory());
    EXPECT_TRUE(p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_stem());
    EXPECT_TRUE(!p.has_extension());
    EXPECT_TRUE(!p.has_parent_path());
    EXPECT_TRUE(!p.is_absolute());

    p = q = "/foo";
    EXPECT_EQ(p.relative_path().string(), "foo");
    EXPECT_EQ(p.parent_path().string(), "/");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_EQ(p.filename(), "foo");
    EXPECT_EQ(p.stem(), "foo");
    EXPECT_EQ(p.extension(), "");
    EXPECT_EQ(p.root_name(), "");
    EXPECT_EQ(p.root_directory(), "/");
    EXPECT_EQ(p.root_path().string(), "/");
    EXPECT_TRUE(p.has_root_path());
    EXPECT_TRUE(!p.has_root_name());
    EXPECT_TRUE(p.has_root_directory());
    EXPECT_TRUE(p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_stem());
    EXPECT_TRUE(!p.has_extension());
    EXPECT_TRUE(p.has_parent_path());
// TODO: fixme
#ifndef BUILD_FOR_WINDOWS
      EXPECT_TRUE(p.is_absolute());
#else
      EXPECT_TRUE(!p.is_absolute());
#endif

    p = q = "/foo/";
    EXPECT_EQ(p.relative_path().string(), "foo/");
    EXPECT_EQ(p.parent_path().string(), "/foo");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_EQ(p.filename(), ".");
    EXPECT_EQ(p.stem(), ".");
    EXPECT_EQ(p.extension(), "");
    EXPECT_EQ(p.root_name(), "");
    EXPECT_EQ(p.root_directory(), "/");
    EXPECT_EQ(p.root_path().string(), "/");
    EXPECT_TRUE(p.has_root_path());
    EXPECT_TRUE(!p.has_root_name());
    EXPECT_TRUE(p.has_root_directory());
    EXPECT_TRUE(p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_stem());
    EXPECT_TRUE(!p.has_extension());
    EXPECT_TRUE(p.has_parent_path());
// TODO: fixme
#ifndef BUILD_FOR_WINDOWS
      EXPECT_TRUE(p.is_absolute());
#else
      EXPECT_TRUE(!p.is_absolute());
#endif

    p = q = "///foo";
    EXPECT_EQ(p.relative_path().string(), "foo");
    EXPECT_EQ(p.parent_path().string(), "/");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_EQ(p.filename(), "foo");
    EXPECT_EQ(p.root_name(), "");
    EXPECT_EQ(p.root_directory(), "/");
    EXPECT_EQ(p.root_path().string(), "/");
    EXPECT_TRUE(p.has_root_path());
    EXPECT_TRUE(!p.has_root_name());
    EXPECT_TRUE(p.has_root_directory());
    EXPECT_TRUE(p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_parent_path());
// TODO: fixme
#ifndef BUILD_FOR_WINDOWS
      EXPECT_TRUE(p.is_absolute());
#else
      EXPECT_TRUE(!p.is_absolute());
#endif

    p = q = "foo/bar";
    EXPECT_TRUE(p.relative_path().string() == "foo/bar");
    EXPECT_TRUE(p.parent_path().string() == "foo");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_TRUE(p.filename() == "bar");
    EXPECT_TRUE(p.stem() == "bar");
    EXPECT_TRUE(p.extension() == "");
    EXPECT_TRUE(p.root_name() == "");
    EXPECT_TRUE(p.root_directory() == "");
    EXPECT_TRUE(p.root_path().string() == "");
    EXPECT_TRUE(!p.has_root_path());
    EXPECT_TRUE(!p.has_root_name());
    EXPECT_TRUE(!p.has_root_directory());
    EXPECT_TRUE(p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_stem());
    EXPECT_TRUE(!p.has_extension());
    EXPECT_TRUE(p.has_parent_path());
    EXPECT_TRUE(!p.is_absolute());

    p = q = "../foo";
    EXPECT_TRUE(p.relative_path().string() == "../foo");
    EXPECT_TRUE(p.parent_path().string() == "..");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_TRUE(p.filename() == "foo");
    EXPECT_TRUE(p.root_name() == "");
    EXPECT_TRUE(p.root_directory() == "");
    EXPECT_TRUE(p.root_path().string() == "");
    EXPECT_TRUE(!p.has_root_path());
    EXPECT_TRUE(!p.has_root_name());
    EXPECT_TRUE(!p.has_root_directory());
    EXPECT_TRUE(p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_parent_path());
    EXPECT_TRUE(!p.is_absolute());

    p = q = "..///foo";
    EXPECT_EQ(p.relative_path().string(), "..///foo");
    EXPECT_EQ(p.parent_path().string(), "..");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_EQ(p.filename(), "foo");
    EXPECT_EQ(p.root_name(), "");
    EXPECT_EQ(p.root_directory(), "");
    EXPECT_EQ(p.root_path().string(), "");
    EXPECT_TRUE(!p.has_root_path());
    EXPECT_TRUE(!p.has_root_name());
    EXPECT_TRUE(!p.has_root_directory());
    EXPECT_TRUE(p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_parent_path());
    EXPECT_TRUE(!p.is_absolute());

    p = q = "/foo/bar";
    EXPECT_TRUE(p.relative_path().string() == "foo/bar");
    EXPECT_TRUE(p.parent_path().string() == "/foo");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_TRUE(p.filename() == "bar");
    EXPECT_TRUE(p.root_name() == "");
    EXPECT_TRUE(p.root_directory() == "/");
    EXPECT_TRUE(p.root_path().string() == "/");
    EXPECT_TRUE(p.has_root_path());
    EXPECT_TRUE(!p.has_root_name());
    EXPECT_TRUE(p.has_root_directory());
    EXPECT_TRUE(p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_parent_path());
// TODO: fixme
#ifndef BUILD_FOR_WINDOWS
      EXPECT_TRUE(p.is_absolute());
#else
      EXPECT_TRUE(!p.is_absolute());
#endif

    // Both POSIX and Windows allow two leading slashs
    // (POSIX meaning is implementation defined)
    EXPECT_EQ(Path("//resource"), "//resource");
    EXPECT_EQ(Path("//resource/"), "//resource/");
    EXPECT_EQ(Path("//resource/foo"), "//resource/foo");

    p = q = Path("//net");
    EXPECT_EQ(p.string(), "//net");
    EXPECT_EQ(p.relative_path().string(), "");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_EQ(p.parent_path().string(), "");
    EXPECT_EQ(p.filename(), "//net");
    EXPECT_EQ(p.root_name(), "//net");
    EXPECT_EQ(p.root_directory(), "");
    EXPECT_EQ(p.root_path().string(), "//net");
    EXPECT_TRUE(p.has_root_path());
    EXPECT_TRUE(p.has_root_name());
    EXPECT_TRUE(!p.has_root_directory());
    EXPECT_TRUE(!p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(!p.has_parent_path());
    EXPECT_TRUE(!p.is_absolute());

    p = q = Path("//net/");
    EXPECT_TRUE(p.relative_path().string() == "");
    EXPECT_TRUE(p.parent_path().string() == "//net");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_TRUE(p.filename() == "/");
    EXPECT_TRUE(p.root_name() == "//net");
    EXPECT_TRUE(p.root_directory() == "/");
    EXPECT_TRUE(p.root_path().string() == "//net/");
    EXPECT_TRUE(p.has_root_path());
    EXPECT_TRUE(p.has_root_name());
    EXPECT_TRUE(p.has_root_directory());
    EXPECT_TRUE(!p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_parent_path());
    EXPECT_TRUE(p.is_absolute());

    p = q = Path("//net/foo");
    EXPECT_TRUE(p.relative_path().string() == "foo");
    EXPECT_TRUE(p.parent_path().string() == "//net/");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_TRUE(p.filename() == "foo");
    EXPECT_TRUE(p.root_name() == "//net");
    EXPECT_TRUE(p.root_directory() == "/");
    EXPECT_TRUE(p.root_path().string() == "//net/");
    EXPECT_TRUE(p.has_root_path());
    EXPECT_TRUE(p.has_root_name());
    EXPECT_TRUE(p.has_root_directory());
    EXPECT_TRUE(p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_parent_path());
    EXPECT_TRUE(p.is_absolute());

    p = q = Path("//net///foo");
    EXPECT_EQ(p.relative_path().string(), "foo");
    EXPECT_EQ(p.parent_path().string(), "//net/");
    EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
    EXPECT_EQ(p.filename(), "foo");
    EXPECT_EQ(p.root_name(), "//net");
    EXPECT_EQ(p.root_directory(), "/");
    EXPECT_EQ(p.root_path().string(), "//net/");
    EXPECT_TRUE(p.has_root_path());
    EXPECT_TRUE(p.has_root_name());
    EXPECT_TRUE(p.has_root_directory());
    EXPECT_TRUE(p.has_relative_path());
    EXPECT_TRUE(p.has_filename());
    EXPECT_TRUE(p.has_parent_path());
    EXPECT_TRUE(p.is_absolute());

    //  ticket 2739, infinite recursion leading to stack overflow, was caused
    //  by failure to handle this case correctly on Windows.
    p = Path(":");
    EXPECT_EQ(p.parent_path().string(), "");
    EXPECT_EQ(p.filename(), ":");
    EXPECT_TRUE(!p.has_parent_path());
    EXPECT_TRUE(p.has_filename());

    //  test some similar cases that both POSIX and Windows should handle identically
    p = Path("c:");
    EXPECT_EQ(p.parent_path().string(), "");
    EXPECT_EQ(p.filename(), "c:");
    EXPECT_TRUE(!p.has_parent_path());
    EXPECT_TRUE(p.has_filename());
    p = Path("cc:");
    EXPECT_EQ(p.parent_path().string(), "");
    EXPECT_EQ(p.filename(), "cc:");
    EXPECT_TRUE(!p.has_parent_path());
    EXPECT_TRUE(p.has_filename());

    //  Windows specific tests
// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
      p = q = Path("c:");
      EXPECT_TRUE(p.relative_path().string() == "");
      EXPECT_TRUE(p.parent_path().string() == "");
      EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
      EXPECT_TRUE(p.filename() == "c:");
      EXPECT_TRUE(p.root_name() == "c:");
      EXPECT_TRUE(p.root_directory() == "");
      EXPECT_TRUE(p.root_path().string() == "c:");
      EXPECT_TRUE(p.has_root_path());
      EXPECT_TRUE(p.has_root_name());
      EXPECT_TRUE(!p.has_root_directory());
      EXPECT_TRUE(!p.has_relative_path());
      EXPECT_TRUE(p.has_filename());
      EXPECT_TRUE(!p.has_parent_path());
      EXPECT_TRUE(!p.is_absolute());

      p = q = Path("c:foo");
      EXPECT_TRUE(p.relative_path().string() == "foo");
      EXPECT_TRUE(p.parent_path().string() == "c:");
      EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
      EXPECT_TRUE(p.filename() == "foo");
      EXPECT_TRUE(p.root_name() == "c:");
      EXPECT_TRUE(p.root_directory() == "");
      EXPECT_TRUE(p.root_path().string() == "c:");
      EXPECT_TRUE(p.has_root_path());
      EXPECT_TRUE(p.has_root_name());
      EXPECT_TRUE(!p.has_root_directory());
      EXPECT_TRUE(p.has_relative_path());
      EXPECT_TRUE(p.has_filename());
      EXPECT_TRUE(p.has_parent_path());
      EXPECT_TRUE(!p.is_absolute());

      p = q = Path("c:/");
      EXPECT_TRUE(p.relative_path().string() == "");
      EXPECT_TRUE(p.parent_path().string() == "c:");
      EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
      EXPECT_TRUE(p.filename() == "/");
      EXPECT_TRUE(p.root_name() == "c:");
      EXPECT_TRUE(p.root_directory() == "/");
      EXPECT_TRUE(p.root_path().string() == "c:/");
      EXPECT_TRUE(p.has_root_path());
      EXPECT_TRUE(p.has_root_name());
      EXPECT_TRUE(p.has_root_directory());
      EXPECT_TRUE(!p.has_relative_path());
      EXPECT_TRUE(p.has_filename());
      EXPECT_TRUE(p.has_parent_path());
      EXPECT_TRUE(p.is_absolute());

      p = q = Path("c:..");
      EXPECT_TRUE(p.relative_path().string() == "..");
      EXPECT_TRUE(p.parent_path().string() == "c:");
      EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
      EXPECT_TRUE(p.filename() == "..");
      EXPECT_TRUE(p.root_name() == "c:");
      EXPECT_TRUE(p.root_directory() == "");
      EXPECT_TRUE(p.root_path().string() == "c:");
      EXPECT_TRUE(p.has_root_path());
      EXPECT_TRUE(p.has_root_name());
      EXPECT_TRUE(!p.has_root_directory());
      EXPECT_TRUE(p.has_relative_path());
      EXPECT_TRUE(p.has_filename());
      EXPECT_TRUE(p.has_parent_path());
      EXPECT_TRUE(!p.is_absolute());

      p = q = Path("c:/foo");
      EXPECT_EQ(p.relative_path().string(), "foo");
      EXPECT_EQ(p.parent_path().string(), "c:/");
      EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
      EXPECT_EQ(p.filename(), "foo");
      EXPECT_EQ(p.root_name(), "c:");
      EXPECT_EQ(p.root_directory(), "/");
      EXPECT_EQ(p.root_path().string(), "c:/");
      EXPECT_TRUE(p.has_root_path());
      EXPECT_TRUE(p.has_root_name());
      EXPECT_TRUE(p.has_root_directory());
      EXPECT_TRUE(p.has_relative_path());
      EXPECT_TRUE(p.has_filename());
      EXPECT_TRUE(p.has_parent_path());
      EXPECT_TRUE(p.is_absolute());

      p = q = Path("c://foo");
      EXPECT_EQ(p.relative_path().string(), "foo");
      EXPECT_EQ(p.parent_path().string(), "c:/");
      EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
      EXPECT_EQ(p.filename(), "foo");
      EXPECT_EQ(p.root_name(), "c:");
      EXPECT_EQ(p.root_directory(), "/");
      EXPECT_EQ(p.root_path().string(), "c:/");
      EXPECT_TRUE(p.has_root_path());
      EXPECT_TRUE(p.has_root_name());
      EXPECT_TRUE(p.has_root_directory());
      EXPECT_TRUE(p.has_relative_path());
      EXPECT_TRUE(p.has_filename());
      EXPECT_TRUE(p.has_parent_path());
      EXPECT_TRUE(p.is_absolute());

      p = q = Path("c:\\foo\\bar");
      EXPECT_EQ(p.relative_path().string(), "foo\\bar");
      EXPECT_EQ(p.parent_path().string(), "c:\\foo");
      EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
      EXPECT_EQ(p.filename(), "bar");
      EXPECT_EQ(p.root_name(), "c:");
      EXPECT_EQ(p.root_directory(), "\\");
      EXPECT_EQ(p.root_path().string(), "c:\\");
      EXPECT_TRUE(p.has_root_path());
      EXPECT_TRUE(p.has_root_name());
      EXPECT_TRUE(p.has_root_directory());
      EXPECT_TRUE(p.has_relative_path());
      EXPECT_TRUE(p.has_filename());
      EXPECT_TRUE(p.has_parent_path());
      EXPECT_TRUE(p.is_absolute());

      p = q = Path("prn:");
      EXPECT_TRUE(p.relative_path().string() == "");
      EXPECT_TRUE(p.parent_path().string() == "");
      EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
      EXPECT_TRUE(p.filename() == "prn:");
      EXPECT_TRUE(p.root_name() == "prn:");
      EXPECT_TRUE(p.root_directory() == "");
      EXPECT_TRUE(p.root_path().string() == "prn:");
      EXPECT_TRUE(p.has_root_path());
      EXPECT_TRUE(p.has_root_name());
      EXPECT_TRUE(!p.has_root_directory());
      EXPECT_TRUE(!p.has_relative_path());
      EXPECT_TRUE(p.has_filename());
      EXPECT_TRUE(!p.has_parent_path());
      EXPECT_TRUE(!p.is_absolute());

      p = q = Path("\\\\net\\\\\\foo");
      EXPECT_EQ(p.relative_path().string(), "foo");
      EXPECT_EQ(p.parent_path().string(), "\\\\net\\");
      EXPECT_EQ(q.remove_filename().string(), p.parent_path().string());
      EXPECT_EQ(p.filename(), "foo");
      EXPECT_EQ(p.root_name(), "\\\\net");
      EXPECT_EQ(p.root_directory(), "\\");
      EXPECT_EQ(p.root_path().string(), "\\\\net\\");
      EXPECT_TRUE(p.has_root_path());
      EXPECT_TRUE(p.has_root_name());
      EXPECT_TRUE(p.has_root_directory());
      EXPECT_TRUE(p.has_relative_path());
      EXPECT_TRUE(p.has_filename());
      EXPECT_TRUE(p.has_parent_path());
      EXPECT_TRUE(p.is_absolute());
#else // POSIX
      EXPECT_EQ(Path("/foo/bar/"), "/foo/bar/");
      EXPECT_EQ(Path("//foo//bar//"), "//foo//bar//");
      EXPECT_EQ(Path("///foo///bar///"), "///foo///bar///");

      p = Path("/usr/local/bin:/usr/bin:/bin");
      EXPECT_TRUE(p.string() == "/usr/local/bin:/usr/bin:/bin");
#endif
}

TEST_F(PathTest, construction) {
    EXPECT_EQ(Path(""), "");

    EXPECT_EQ(Path("foo"), "foo");
    EXPECT_EQ(Path("f"), "f");

    EXPECT_EQ(Path("foo/"), "foo/");
    EXPECT_EQ(Path("f/"), "f/");
    EXPECT_EQ(Path("foo/.."), "foo/..");
    EXPECT_EQ(Path("foo/../"), "foo/../");
    EXPECT_EQ(Path("foo/bar/../.."), "foo/bar/../..");
    EXPECT_EQ(Path("foo/bar/../../"), "foo/bar/../../");
    EXPECT_EQ(Path("/"), "/");
    EXPECT_EQ(Path("/f"), "/f");

    EXPECT_EQ(Path("/foo"), "/foo");
    EXPECT_EQ(Path("/foo/bar/"), "/foo/bar/");
    EXPECT_EQ(Path("//foo//bar//"), "//foo//bar//");
    EXPECT_EQ(Path("///foo///bar///"), "///foo///bar///");
    EXPECT_EQ(Path("\\/foo\\/bar\\/"), "\\/foo\\/bar\\/");
    EXPECT_EQ(Path("\\//foo\\//bar\\//"), "\\//foo\\//bar\\//");

// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
      EXPECT_EQ(Path("c:") / "foo", "c:foo");
      EXPECT_EQ(Path("c:") / "/foo", "c:/foo");

      EXPECT_EQ(Path("\\foo\\bar\\"), "\\foo\\bar\\");
      EXPECT_EQ(Path("\\\\foo\\\\bar\\\\"), "\\\\foo\\\\bar\\\\");
      EXPECT_EQ(Path("\\\\\\foo\\\\\\bar\\\\\\"), "\\\\\\foo\\\\\\bar\\\\\\");

      EXPECT_EQ(Path("\\"), "\\");
      EXPECT_EQ(Path("\\f"), "\\f");
      EXPECT_EQ(Path("\\foo"), "\\foo");
      EXPECT_EQ(Path("foo\\bar"), "foo\\bar");
      EXPECT_EQ(Path("foo bar"), "foo bar");
      EXPECT_EQ(Path("c:"), "c:");
      EXPECT_EQ(Path("c:/"), "c:/");
      EXPECT_EQ(Path("c:."), "c:.");
      EXPECT_EQ(Path("c:./foo"), "c:./foo");
      EXPECT_EQ(Path("c:.\\foo"), "c:.\\foo");
      EXPECT_EQ(Path("c:.."), "c:..");
      EXPECT_EQ(Path("c:/."), "c:/.");
      EXPECT_EQ(Path("c:/.."), "c:/..");
      EXPECT_EQ(Path("c:/../"), "c:/../");
      EXPECT_EQ(Path("c:\\..\\"), "c:\\..\\");
      EXPECT_EQ(Path("c:/../.."), "c:/../..");
      EXPECT_EQ(Path("c:/../foo"), "c:/../foo");
      EXPECT_EQ(Path("c:\\..\\foo"), "c:\\..\\foo");
      EXPECT_EQ(Path("c:../foo"), "c:../foo");
      EXPECT_EQ(Path("c:..\\foo"), "c:..\\foo");
      EXPECT_EQ(Path("c:/../../foo"), "c:/../../foo");
      EXPECT_EQ(Path("c:\\..\\..\\foo"), "c:\\..\\..\\foo");
      EXPECT_EQ(Path("c:foo/.."), "c:foo/..");
      EXPECT_EQ(Path("c:/foo/.."), "c:/foo/..");
      EXPECT_EQ(Path("c:/..foo"), "c:/..foo");
      EXPECT_EQ(Path("c:foo"), "c:foo");
      EXPECT_EQ(Path("c:/foo"), "c:/foo");
      EXPECT_EQ(Path("\\\\netname"), "\\\\netname");
      EXPECT_EQ(Path("\\\\netname\\"), "\\\\netname\\");
      EXPECT_EQ(Path("\\\\netname\\foo"), "\\\\netname\\foo");
      EXPECT_EQ(Path("c:/foo"), "c:/foo");
      EXPECT_EQ(Path("prn:"), "prn:");
#endif

    EXPECT_EQ(Path("foo/bar"), "foo/bar");
    EXPECT_EQ(Path("a/b"), "a/b");  // probe for length effects
    EXPECT_EQ(Path(".."), "..");
    EXPECT_EQ(Path("../.."), "../..");
    EXPECT_EQ(Path("/.."), "/..");
    EXPECT_EQ(Path("/../.."), "/../..");
    EXPECT_EQ(Path("../foo"), "../foo");
    EXPECT_EQ(Path("foo/.."), "foo/..");
    EXPECT_EQ(Path("foo/..bar"), "foo/..bar");
    EXPECT_EQ(Path("../f"), "../f");
    EXPECT_EQ(Path("/../f"), "/../f");
    EXPECT_EQ(Path("f/.."), "f/..");
    EXPECT_EQ(Path("foo/../.."), "foo/../..");
    EXPECT_EQ(Path("foo/../../.."), "foo/../../..");
    EXPECT_EQ(Path("foo/../bar"), "foo/../bar");
    EXPECT_EQ(Path("foo/bar/.."), "foo/bar/..");
    EXPECT_EQ(Path("foo/bar/../.."), "foo/bar/../..");
    EXPECT_EQ(Path("foo/bar/../blah"), "foo/bar/../blah");
    EXPECT_EQ(Path("f/../b"), "f/../b");
    EXPECT_EQ(Path("f/b/.."), "f/b/..");
    EXPECT_EQ(Path("f/b/../a"), "f/b/../a");
    EXPECT_EQ(Path("foo/bar/blah/../.."), "foo/bar/blah/../..");
    EXPECT_EQ(Path("foo/bar/blah/../../bletch"), "foo/bar/blah/../../bletch");
    EXPECT_EQ(Path("..."), "...");
    EXPECT_EQ(Path("...."), "....");
    EXPECT_EQ(Path("foo/..."), "foo/...");
    EXPECT_EQ(Path("abc."), "abc.");
    EXPECT_EQ(Path("abc.."), "abc..");
    EXPECT_EQ(Path("foo/abc."), "foo/abc.");
    EXPECT_EQ(Path("foo/abc.."), "foo/abc..");

    EXPECT_EQ(Path(".abc"), ".abc");
    EXPECT_EQ(Path("a.c"), "a.c");
    EXPECT_EQ(Path("..abc"), "..abc");
    EXPECT_EQ(Path("a..c"), "a..c");
    EXPECT_EQ(Path("foo/.abc"), "foo/.abc");
    EXPECT_EQ(Path("foo/a.c"), "foo/a.c");
    EXPECT_EQ(Path("foo/..abc"), "foo/..abc");
    EXPECT_EQ(Path("foo/a..c"), "foo/a..c");

    EXPECT_EQ(Path("."), ".");
    EXPECT_EQ(Path("./foo"), "./foo");
    EXPECT_EQ(Path("./.."), "./..");
    EXPECT_EQ(Path("./../foo"), "./../foo");
    EXPECT_EQ(Path("foo/."), "foo/.");
    EXPECT_EQ(Path("../."), "../.");
    EXPECT_EQ(Path("./."), "./.");
    EXPECT_EQ(Path("././."), "././.");
    EXPECT_EQ(Path("./foo/."), "./foo/.");
    EXPECT_EQ(Path("foo/./bar"), "foo/./bar");
    EXPECT_EQ(Path("foo/./."), "foo/./.");
    EXPECT_EQ(Path("foo/./.."), "foo/./..");
    EXPECT_EQ(Path("foo/./../bar"), "foo/./../bar");
    EXPECT_EQ(Path("foo/../."), "foo/../.");
    EXPECT_EQ(Path("././.."), "././..");
    EXPECT_EQ(Path("./../."), "./../.");
    EXPECT_EQ(Path(".././."), ".././.");
}

namespace {

void append_test_aux(const Path& p, const std::string& s, const std::string& expect) {
    EXPECT_EQ((p / Path(s)).string(), expect);
    EXPECT_EQ((p / s.c_str()).string(), expect);
    EXPECT_EQ((p / s).string(), expect);
    Path x(p);
    x.append(s.begin(), s.end());
    EXPECT_EQ(x.string(), expect);
}

} // namespace

TEST_F(PathTest, append) {
    EXPECT_EQ(Path("") / "", "");
    append_test_aux("", "", "");

    EXPECT_EQ(Path("") / "/", "/");
    append_test_aux("", "/", "/");

    EXPECT_EQ(Path("") / "bar", "bar");
    append_test_aux("", "bar", "bar");

    EXPECT_EQ(Path("") / "/bar", "/bar");
    append_test_aux("", "/bar", "/bar");

    EXPECT_EQ(Path("/") / "", "/");
    append_test_aux("/", "", "/");

    EXPECT_EQ(Path("/") / "/", "//");
    append_test_aux("/", "/", "//");

    EXPECT_EQ(Path("/") / "bar", "/bar");
    append_test_aux("/", "bar", "/bar");

    EXPECT_EQ(Path("/") / "/bar", "//bar");
    append_test_aux("/", "/bar", "//bar");

    EXPECT_EQ(Path("foo") / "", "foo");
    append_test_aux("foo", "", "foo");

    EXPECT_EQ(Path("foo") / "/", "foo/");
    append_test_aux("foo", "/", "foo/");

    EXPECT_EQ(Path("foo") / "/bar", "foo/bar");
    append_test_aux("foo", "/bar", "foo/bar");

    EXPECT_EQ(Path("foo/") / "", "foo/");
    append_test_aux("foo/", "", "foo/");

    EXPECT_EQ(Path("foo/") / "/", "foo//");
    append_test_aux("foo/", "/", "foo//");

    EXPECT_EQ(Path("foo/") / "bar", "foo/bar");
    append_test_aux("foo/", "bar", "foo/bar");


// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
      EXPECT_EQ(Path("foo") / "bar", "foo\\bar");
      append_test_aux("foo", "bar", "foo\\bar");

      EXPECT_EQ(Path("foo\\") / "\\bar", "foo\\\\bar");
      append_test_aux("foo\\", "\\bar", "foo\\\\bar");

      // hand created test case specific to Windows
      EXPECT_EQ(Path("c:") / "bar", "c:bar");
      append_test_aux("c:", "bar", "c:bar");
#else
      EXPECT_EQ(Path("foo") / "bar", "foo/bar");
      append_test_aux("foo", "bar", "foo/bar");
#endif

    // ticket #6819
    union
    {
      char a[1];
      char b[3];
    } u;

    u.b[0] = 'a';
    u.b[1] = 'b';
    u.b[2] = '\0';

    Path p6819;
    p6819 /= u.a;
    EXPECT_EQ(p6819, Path("ab"));
}

TEST_F(PathTest, self_assign_and_append) {
    Path p;

    p = "snafubar";
    EXPECT_EQ(p = p, "snafubar");

    p = "snafubar";
    p = p.c_str();
    EXPECT_EQ(p, "snafubar");

    p = "snafubar";
    p.assign(p.c_str(), Path::codecvt());
    EXPECT_EQ(p, "snafubar");

    p = "snafubar";
    EXPECT_EQ(p = p.c_str()+5, "bar");

    p = "snafubar";
    EXPECT_EQ(p.assign(p.c_str() + 5, p.c_str() + 7), "ba");

    p = "snafubar";
    p /= p;
    EXPECT_EQ(p, "snafubar" BOOST_DIR_SEP "snafubar");

    p = "snafubar";
    p /= p.c_str();
    EXPECT_EQ(p, "snafubar" BOOST_DIR_SEP "snafubar");

    p = "snafubar";
    p.append(p.c_str(), Path::codecvt());
    EXPECT_EQ(p, "snafubar" BOOST_DIR_SEP "snafubar");

    p = "snafubar";
    EXPECT_EQ(p.append(p.c_str() + 5, p.c_str() + 7), "snafubar" BOOST_DIR_SEP "ba");
}

// TODO: skipped void name_function_tests() from original source code
// need to return it???

TEST_F(PathTest, replace_extension) {
    EXPECT_TRUE(Path().replace_extension().empty());
    EXPECT_TRUE(Path().replace_extension("a") == ".a");
    EXPECT_TRUE(Path().replace_extension("a.") == ".a.");
    EXPECT_TRUE(Path().replace_extension(".a") == ".a");
    EXPECT_TRUE(Path().replace_extension("a.txt") == ".a.txt");
    // see the rationale in html docs for explanation why this works:
    EXPECT_TRUE(Path().replace_extension(".txt") == ".txt");

    EXPECT_TRUE(Path("a.txt").replace_extension() == "a");
    EXPECT_TRUE(Path("a.txt").replace_extension("") == "a");
    EXPECT_TRUE(Path("a.txt").replace_extension(".") == "a.");
    EXPECT_TRUE(Path("a.txt").replace_extension(".tex") == "a.tex");
    EXPECT_TRUE(Path("a.txt").replace_extension("tex") == "a.tex");
    EXPECT_TRUE(Path("a.").replace_extension(".tex") == "a.tex");
    EXPECT_TRUE(Path("a.").replace_extension("tex") == "a.tex");
    EXPECT_TRUE(Path("a").replace_extension(".txt") == "a.txt");
    EXPECT_TRUE(Path("a").replace_extension("txt") == "a.txt");
    EXPECT_TRUE(Path("a.b.txt").replace_extension(".tex") == "a.b.tex");
    EXPECT_TRUE(Path("a.b.txt").replace_extension("tex") == "a.b.tex");
    EXPECT_TRUE(Path("a/b").replace_extension(".c") == "a/b.c");
    EXPECT_EQ(Path("a.txt/b").replace_extension(".c"), "a.txt/b.c");    // ticket 4702
    EXPECT_TRUE(Path("foo.txt").replace_extension("exe") == "foo.exe"); // ticket 5118
    EXPECT_TRUE(Path("foo.txt").replace_extension(".tar.bz2")
                                                    == "foo.tar.bz2");  // ticket 5118
  }

TEST_F(PathTest, make_preferred) {
// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
    EXPECT_TRUE(Path("//abc\\def/ghi").make_preferred().native() == Path("\\\\abc\\def\\ghi").native());
#else
    EXPECT_TRUE(Path("//abc\\def/ghi").make_preferred().native() == Path("//abc\\def/ghi").native());
#endif
}

TEST_F(PathTest, lexically_normal) {
    //  Note: lexically_lexically_normal() uses /= to build up some results, so these results will
    //  have the platform's preferred separator. Since that is immaterial to the correct
    //  functioning of lexically_lexically_normal(), the test results are converted to generic form,
    //  and the expected results are also given in generic form. Otherwise many of the
    //  tests would incorrectly be reported as failing on Windows.

    EXPECT_EQ(Path("").lexically_normal().generic_path(), "");
    EXPECT_EQ(Path("/").lexically_normal().generic_path(), "/");
    EXPECT_EQ(Path("//").lexically_normal().generic_path(), "//");
    EXPECT_EQ(Path("///").lexically_normal().generic_path(), "/");
    EXPECT_EQ(Path("f").lexically_normal().generic_path(), "f");
    EXPECT_EQ(Path("foo").lexically_normal().generic_path(), "foo");
    EXPECT_EQ(Path("foo/").lexically_normal().generic_path(), "foo/.");
    EXPECT_EQ(Path("f/").lexically_normal().generic_path(), "f/.");
    EXPECT_EQ(Path("/foo").lexically_normal().generic_path(), "/foo");
    EXPECT_EQ(Path("foo/bar").lexically_normal().generic_path(), "foo/bar");
    EXPECT_EQ(Path("..").lexically_normal().generic_path(), "..");
    EXPECT_EQ(Path("../..").lexically_normal().generic_path(), "../..");
    EXPECT_EQ(Path("/..").lexically_normal().generic_path(), "/..");
    EXPECT_EQ(Path("/../..").lexically_normal().generic_path(), "/../..");
    EXPECT_EQ(Path("../foo").lexically_normal().generic_path(), "../foo");
    EXPECT_EQ(Path("foo/..").lexically_normal().generic_path(), ".");
    EXPECT_EQ(Path("foo/../").lexically_normal().generic_path(), "./.");
    EXPECT_EQ((Path("foo") / "..").lexically_normal().generic_path() , ".");
    EXPECT_EQ(Path("foo/...").lexically_normal().generic_path(), "foo/...");
    EXPECT_EQ(Path("foo/.../").lexically_normal().generic_path(), "foo/.../.");
    EXPECT_EQ(Path("foo/..bar").lexically_normal().generic_path(), "foo/..bar");
    EXPECT_EQ(Path("../f").lexically_normal().generic_path(), "../f");
    EXPECT_EQ(Path("/../f").lexically_normal().generic_path(), "/../f");
    EXPECT_EQ(Path("f/..").lexically_normal().generic_path(), ".");
    EXPECT_EQ((Path("f") / "..").lexically_normal().generic_path() , ".");
    EXPECT_EQ(Path("foo/../..").lexically_normal().generic_path(), "..");
    EXPECT_EQ(Path("foo/../../").lexically_normal().generic_path(), "../.");
    EXPECT_EQ(Path("foo/../../..").lexically_normal().generic_path(), "../..");
    EXPECT_EQ(Path("foo/../../../").lexically_normal().generic_path(), "../../.");
    EXPECT_EQ(Path("foo/../bar").lexically_normal().generic_path(), "bar");
    EXPECT_EQ(Path("foo/../bar/").lexically_normal().generic_path(), "bar/.");
    EXPECT_EQ(Path("foo/bar/..").lexically_normal().generic_path(), "foo");
    EXPECT_EQ(Path("foo/./bar/..").lexically_normal().generic_path(), "foo");
    EXPECT_EQ(Path("foo/bar/../").lexically_normal().generic_path(), "foo/.");
    EXPECT_EQ(Path("foo/./bar/../").lexically_normal().generic_path(), "foo/.");
    EXPECT_EQ(Path("foo/bar/../..").lexically_normal().generic_path(), ".");
    EXPECT_EQ(Path("foo/bar/../../").lexically_normal().generic_path(), "./.");
    EXPECT_EQ(Path("foo/bar/../blah").lexically_normal().generic_path(), "foo/blah");
    EXPECT_EQ(Path("f/../b").lexically_normal().generic_path(), "b");
    EXPECT_EQ(Path("f/b/..").lexically_normal().generic_path(), "f");
    EXPECT_EQ(Path("f/b/../").lexically_normal().generic_path(), "f/.");
    EXPECT_EQ(Path("f/b/../a").lexically_normal().generic_path(), "f/a");
    EXPECT_EQ(Path("foo/bar/blah/../..").lexically_normal().generic_path(), "foo");
    EXPECT_EQ(Path("foo/bar/blah/../../bletch").lexically_normal().generic_path(), "foo/bletch");
    EXPECT_EQ(Path("//net").lexically_normal().generic_path(), "//net");
    EXPECT_EQ(Path("//net/").lexically_normal().generic_path(), "//net/");
    EXPECT_EQ(Path("//..net").lexically_normal().generic_path(), "//..net");
    EXPECT_EQ(Path("//net/..").lexically_normal().generic_path(), "//net/..");
    EXPECT_EQ(Path("//net/foo").lexically_normal().generic_path(), "//net/foo");
    EXPECT_EQ(Path("//net/foo/").lexically_normal().generic_path(), "//net/foo/.");
    EXPECT_EQ(Path("//net/foo/..").lexically_normal().generic_path(), "//net/");
    EXPECT_EQ(Path("//net/foo/../").lexically_normal().generic_path(), "//net/.");

    EXPECT_EQ(Path("/net/foo/bar").lexically_normal().generic_path(), "/net/foo/bar");
    EXPECT_EQ(Path("/net/foo/bar/").lexically_normal().generic_path(), "/net/foo/bar/.");
    EXPECT_EQ(Path("/net/foo/..").lexically_normal().generic_path(), "/net");
    EXPECT_EQ(Path("/net/foo/../").lexically_normal().generic_path(), "/net/.");

    EXPECT_EQ(Path("//net//foo//bar").lexically_normal().generic_path(), "//net/foo/bar");
    EXPECT_EQ(Path("//net//foo//bar//").lexically_normal().generic_path(), "//net/foo/bar/.");
    EXPECT_EQ(Path("//net//foo//..").lexically_normal().generic_path(), "//net/");
    EXPECT_EQ(Path("//net//foo//..//").lexically_normal().generic_path(), "//net/.");

    EXPECT_EQ(Path("///net///foo///bar").lexically_normal().generic_path(), "/net/foo/bar");
    EXPECT_EQ(Path("///net///foo///bar///").lexically_normal().generic_path(), "/net/foo/bar/.");
    EXPECT_EQ(Path("///net///foo///..").lexically_normal().generic_path(), "/net");
    EXPECT_EQ(Path("///net///foo///..///").lexically_normal().generic_path(), "/net/.");

// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
      EXPECT_EQ(Path("c:..").lexically_normal().generic_path(), "c:..");
      EXPECT_EQ(Path("c:foo/..").lexically_normal().generic_path(), "c:");

      EXPECT_EQ(Path("c:foo/../").lexically_normal().generic_path(), "c:.");

      EXPECT_EQ(Path("c:/foo/..").lexically_normal().generic_path(), "c:/");
      EXPECT_EQ(Path("c:/foo/../").lexically_normal().generic_path(), "c:/.");
      EXPECT_EQ(Path("c:/..").lexically_normal().generic_path(), "c:/..");
      EXPECT_EQ(Path("c:/../").lexically_normal().generic_path(), "c:/../.");
      EXPECT_EQ(Path("c:/../..").lexically_normal().generic_path(), "c:/../..");
      EXPECT_EQ(Path("c:/../../").lexically_normal().generic_path(), "c:/../../.");
      EXPECT_EQ(Path("c:/../foo").lexically_normal().generic_path(), "c:/../foo");
      EXPECT_EQ(Path("c:/../foo/").lexically_normal().generic_path(), "c:/../foo/.");
      EXPECT_EQ(Path("c:/../../foo").lexically_normal().generic_path(), "c:/../../foo");
      EXPECT_EQ(Path("c:/../../foo/").lexically_normal().generic_path(), "c:/../../foo/.");
      EXPECT_EQ(Path("c:/..foo").lexically_normal().generic_path(), "c:/..foo");
#else
      EXPECT_EQ(Path("c:..").lexically_normal(), "c:..");
      EXPECT_EQ(Path("c:foo/..").lexically_normal(), ".");
      EXPECT_EQ(Path("c:foo/../").lexically_normal(), "./.");
      EXPECT_EQ(Path("c:/foo/..").lexically_normal(), "c:");
      EXPECT_EQ(Path("c:/foo/../").lexically_normal(), "c:/.");
      EXPECT_EQ(Path("c:/..").lexically_normal(), ".");
      EXPECT_EQ(Path("c:/../").lexically_normal(), "./.");
      EXPECT_EQ(Path("c:/../..").lexically_normal(), "..");
      EXPECT_EQ(Path("c:/../../").lexically_normal(), "../.");
      EXPECT_EQ(Path("c:/../foo").lexically_normal(), "foo");
      EXPECT_EQ(Path("c:/../foo/").lexically_normal(), "foo/.");
      EXPECT_EQ(Path("c:/../../foo").lexically_normal(), "../foo");
      EXPECT_EQ(Path("c:/../../foo/").lexically_normal(), "../foo/.");
      EXPECT_EQ(Path("c:/..foo").lexically_normal(), "c:/..foo");
#endif
}

TEST_F(PathTest, lexically_relative) {
    EXPECT_TRUE(Path("").lexically_relative("") == "");
    EXPECT_TRUE(Path("").lexically_relative("/foo") == "");
    EXPECT_TRUE(Path("/foo").lexically_relative("") == "");
    EXPECT_TRUE(Path("/foo").lexically_relative("/foo") == ".");
    EXPECT_TRUE(Path("").lexically_relative("foo") == "");
    EXPECT_TRUE(Path("foo").lexically_relative("") == "");
    EXPECT_TRUE(Path("foo").lexically_relative("foo") == ".");

    EXPECT_TRUE(Path("a/b/c").lexically_relative("a") == "b/c");
    EXPECT_TRUE(Path("a//b//c").lexically_relative("a") == "b/c");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("a/b") == "c");
    EXPECT_TRUE(Path("a///b//c").lexically_relative("a//b") == "c");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("a/b/c") == ".");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("a/b/c/x") == "..");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("a/b/c/x/y") == "../..");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("a/x") == "../b/c");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("a/b/x") == "../c");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("a/x/y") == "../../b/c");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("a/b/x/y") == "../../c");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("a/b/c/x/y/z") == "../../..");

    // paths unrelated except first element, and first element is root directory
    EXPECT_TRUE(Path("/a/b/c").lexically_relative("/x") == "../a/b/c");
    EXPECT_TRUE(Path("/a/b/c").lexically_relative("/x/y") == "../../a/b/c");
    EXPECT_TRUE(Path("/a/b/c").lexically_relative("/x/y/z") == "../../../a/b/c");

    // paths unrelated
    EXPECT_TRUE(Path("a/b/c").lexically_relative("x") == "");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("x/y") == "");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("x/y/z") == "");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("/x") == "");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("/x/y") == "");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("/x/y/z") == "");
    EXPECT_TRUE(Path("a/b/c").lexically_relative("/a/b/c") == "");

    // TODO: add some Windows-only test cases that probe presence or absence of
    // drive specifier-and root-directory

    //  Some tests from Jamie Allsop's paper
    EXPECT_TRUE(Path("/a/d").lexically_relative("/a/b/c") == "../../d");
    EXPECT_TRUE(Path("/a/b/c").lexically_relative("/a/d") == "../b/c");

// TODO: fixme
  #ifdef BOOST_WINDOWS_API
    EXPECT_TRUE(Path("c:\\y").lexically_relative("c:\\x") == "../y");
  #else
    EXPECT_TRUE(Path("c:\\y").lexically_relative("c:\\x") == "");
  #endif

    EXPECT_TRUE(Path("d:\\y").lexically_relative("c:\\x") == "");

    //  From issue #1976
    EXPECT_TRUE(Path("/foo/new").lexically_relative("/foo/bar") == "../new");
  }

TEST_F(PathTest, lexically_proximate) {
    // paths unrelated
    EXPECT_TRUE(Path("a/b/c").lexically_proximate("x") == "a/b/c");
}

namespace {

inline void odr_use(const Path::value_type& c) {
    static const Path::value_type dummy = '\0';
    EXPECT_TRUE(&c != &dummy);
}

} // namespace

TEST_F(PathTest, odr_use) {
    odr_use(Path::separator);
    odr_use(Path::preferred_separator);
    odr_use(Path::dot);
}

namespace {

Path p1("fe/fi/fo/fum");
Path p2(p1);
Path p3;
Path p4("foobar");
Path p5;

} // namespace

TEST_F(PathTest, misc) {
  EXPECT_TRUE(p1.string() != p3.string());
  p3 = p2;
  EXPECT_TRUE(p1.string() == p3.string());

  Path p04("foobar");
  EXPECT_TRUE(p04.string() == "foobar");
  p04 = p04; // self-assignment
  EXPECT_TRUE(p04.string() == "foobar");

  std::string s1("//:somestring");  // this used to be treated specially

  // check the Path member templates
  p5.assign(s1.begin(), s1.end());

  EXPECT_EQ(p5.string(), "//:somestring");
  p5 = s1;
  EXPECT_EQ(p5.string(), "//:somestring");

  // this code, courtesy of David Whetstone, detects a now fixed bug that
  // derefereced the end iterator (assuming debug build with checked itors)
  std::vector<char> v1;
  p5.assign(v1.begin(), v1.end());
  std::string s2(v1.begin(), v1.end());
  EXPECT_EQ(p5.string(), s2);
  p5.assign(s1.begin(), s1.begin() + 1);
  EXPECT_EQ(p5.string(), "/");

  EXPECT_TRUE(p1 != p4);
  EXPECT_TRUE(p1.string() == p2.string());
  EXPECT_TRUE(p1.string() == p3.string());
  EXPECT_TRUE(Path("foo").filename() == "foo");
  EXPECT_TRUE(Path("foo").parent_path().string() == "");
  EXPECT_TRUE(p1.filename() == "fum");
  EXPECT_TRUE(p1.parent_path().string() == "fe/fi/fo");
  EXPECT_TRUE(Path("").empty() == true);
  EXPECT_TRUE(Path("foo").empty() == false);
}

namespace {

class error_codecvt : public std::codecvt<wchar_t, char, std::mbstate_t> {
public:
explicit error_codecvt()
    : std::codecvt<wchar_t, char, std::mbstate_t>() {}

protected:
    virtual bool do_always_noconv() const throw() { return false; }
    virtual int do_encoding() const throw() { return 0; }

    virtual std::codecvt_base::result do_in(std::mbstate_t&,
            const char*, const char*, const char*&,
            wchar_t*, wchar_t*, wchar_t*&) const {
        static std::codecvt_base::result r = std::codecvt_base::noconv;
        if (r == std::codecvt_base::partial) r = std::codecvt_base::error;
        else if (r == std::codecvt_base::error) r = std::codecvt_base::noconv;
        else r = std::codecvt_base::partial;
        return r;
    }

    virtual std::codecvt_base::result do_out(std::mbstate_t &,
            const wchar_t*, const wchar_t*, const wchar_t*&,
            char*, char*, char*&) const {
        static std::codecvt_base::result r = std::codecvt_base::noconv;
        if (r == std::codecvt_base::partial) r = std::codecvt_base::error;
        else if (r == std::codecvt_base::error) r = std::codecvt_base::noconv;
        else r = std::codecvt_base::partial;
        return r;
    }

    virtual std::codecvt_base::result do_unshift(std::mbstate_t&, char*, char*, char* &) const  { return ok; }
    virtual int do_length(std::mbstate_t &, const char*, const char*, std::size_t) const  { return 0; }
    virtual int do_max_length() const throw () { return 0; }
};

} // namespace

TEST_F(PathTest, error_handling) {
    std::locale global_loc = std::locale();
    std::locale loc(global_loc, new error_codecvt);
    std::locale old_loc = Path::imbue(loc);

    //  These tests rely on a Path constructor that fails in the locale conversion.
    //  Thus construction has to call codecvt. Force that by using a narrow string
    //  for Windows, and a wide string for POSIX.
// TODO: FIXME
#   ifdef BOOST_WINDOWS_API
#     define STRING_FOO_ "foo"
#   else
#     define STRING_FOO_ L"foo"
#   endif

    {
      bool exception_thrown (false);
      try { Path(STRING_FOO_); }
      catch (const std::system_error& ex)
      {
        exception_thrown = true;
        ASSERT_EQ(ex.code(), std::error_code(std::codecvt_base::partial, io::codecvt_error_category()));
      }
      catch (...) { ASSERT_TRUE(false); }
      ASSERT_TRUE(exception_thrown);
    }

    {
      bool exception_thrown (false);
      try { Path(STRING_FOO_); }
      catch (const std::system_error& ex)
      {
        exception_thrown = true;
        ASSERT_EQ(ex.code(), std::error_code(std::codecvt_base::error, io::codecvt_error_category()));
      }
      catch (...) { ASSERT_TRUE(false); }
      ASSERT_TRUE(exception_thrown);
    }

    {
      bool exception_thrown (false);
      try { Path(STRING_FOO_); }
      catch (const std::system_error & ex)
      {
        exception_thrown = true;
        ASSERT_EQ(ex.code(), std::error_code(std::codecvt_base::noconv, io::codecvt_error_category()));
      }
      catch (...) { ASSERT_TRUE(false); }
      ASSERT_TRUE(exception_thrown);
    }

    // restoring original locale
    Path::imbue(old_loc);
}

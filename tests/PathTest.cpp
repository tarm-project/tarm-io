#include "UTCommon.h"

#include "io/detail/ConstexprString.h"

#include "io/Path.h"

#include <iterator>
#include <string>

// Workaround for GTest printers bug and path
// https://github.com/google/googlemock/issues/170
namespace boost {
    namespace filesystem {
        void PrintTo(const boost::filesystem::path& path, std::ostream* os) {
            *os << path;
        }
    }
}

// TODO: fixme
#ifdef BOOST_WINDOWS_API
# define BOOST_DIR_SEP "\\"
#else
# define BOOST_DIR_SEP "/"
#endif

using boost::filesystem::path;

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
};

TEST_F(PathTest, test_constructors) {
    path x0;                                           // default constructor
    EXPECT_EQ(x0, L"");
    EXPECT_EQ(x0.native().size(), 0U);

    path x1(m_l.begin(), m_l.end());                       // iterator range char
    EXPECT_EQ(x1, L"string");
    EXPECT_EQ(x1.native().size(), 6U);

    path x2(x1);                                       // copy constructor
    EXPECT_EQ(x2, L"string");
    EXPECT_EQ(x2.native().size(), 6U);

    path x3(m_wl.begin(), m_wl.end());                     // iterator range wchar_t
    EXPECT_EQ(x3, L"wstring");
    EXPECT_EQ(x3.native().size(), 7U);

    // contiguous containers
    path x4(std::string("std::string"));                    // std::string
    EXPECT_EQ(x4, L"std::string");
    EXPECT_EQ(x4.native().size(), 11U);

    path x5(std::wstring(L"std::wstring"));                 // std::wstring
    EXPECT_EQ(x5, L"std::wstring");
    EXPECT_EQ(x5.native().size(), 12U);

    path x4v(m_v);                                       // std::vector<char>
    EXPECT_EQ(x4v, L"fuz");
    EXPECT_EQ(x4v.native().size(), 3U);

    path x5v(m_wv);                                      // std::vector<wchar_t>
    EXPECT_EQ(x5v, L"wfuz");
    EXPECT_EQ(x5v.native().size(), 4U);

    path x6("array char");                             // array char
    EXPECT_EQ(x6, L"array char");
    EXPECT_EQ(x6.native().size(), 10U);

    path x7(L"array wchar_t");                         // array wchar_t
    EXPECT_EQ(x7, L"array wchar_t");
    EXPECT_EQ(x7.native().size(), 13U);

    char char_array[100];
    std::strcpy(char_array, "big array char");
    path x6o(char_array);                              // array char, only partially full
    EXPECT_EQ(x6o, L"big array char");
    EXPECT_EQ(x6o.native().size(), 14U);

    wchar_t wchar_array[100];
    std::wcscpy(wchar_array, L"big array wchar_t");
    path x7o(wchar_array);                             // array char, only partially full
    EXPECT_EQ(x7o, L"big array wchar_t");
    EXPECT_EQ(x7o.native().size(), 17U);

    path x8(m_s.c_str());                                // const char* null terminated
    EXPECT_EQ(x8, L"string");
    EXPECT_EQ(x8.native().size(), 6U);

    path x9(m_ws.c_str());                               // const wchar_t* null terminated
    EXPECT_EQ(x9, L"wstring");
    EXPECT_EQ(x9.native().size(), 7U);

    path x8nc(const_cast<char*>(m_s.c_str()));           // char* null terminated
    EXPECT_EQ(x8nc, L"string");
    EXPECT_EQ(x8nc.native().size(), 6U);

    path x9nc(const_cast<wchar_t*>(m_ws.c_str()));       // wchar_t* null terminated
    EXPECT_EQ(x9nc, L"wstring");
    EXPECT_EQ(x9nc.native().size(), 7U);

    // non-contiguous containers
    path x10(m_l);                                       // std::list<char>
    EXPECT_EQ(x10, L"string");
    EXPECT_EQ(x10.native().size(), 6U);

    path xll(m_wl);                                      // std::list<wchar_t>
    EXPECT_EQ(xll, L"wstring");
    EXPECT_EQ(xll.native().size(), 7U);
}

TEST_F(PathTest, iterator_tests) {
    path itr_ck = "";
    path::const_iterator itr = itr_ck.begin();
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

    itr_ck = path("/foo");
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

        itr_ck = path("c:");
        EXPECT_TRUE(*itr_ck.begin() == std::string("c:"));
        EXPECT_TRUE(next(itr_ck.begin()) == itr_ck.end());
        EXPECT_TRUE(prior(itr_ck.end()) == itr_ck.begin());
        EXPECT_TRUE(*prior(itr_ck.end()) == std::string("c:"));

        itr_ck = path("c:/");
        EXPECT_TRUE(*itr_ck.begin() == std::string("c:"));
        EXPECT_TRUE(*next(itr_ck.begin()) == std::string("/"));
        EXPECT_TRUE(next(next(itr_ck.begin())) == itr_ck.end());
        EXPECT_TRUE(prior(prior(itr_ck.end())) == itr_ck.begin());
        EXPECT_TRUE(*prior(itr_ck.end()) == std::string("/"));
        EXPECT_TRUE(*prior(prior(itr_ck.end())) == std::string("c:"));

        itr_ck = path("c:foo");
        EXPECT_TRUE(*itr_ck.begin() == std::string("c:"));
        EXPECT_TRUE(*next(itr_ck.begin()) == std::string("foo"));
        EXPECT_TRUE(next(next(itr_ck.begin())) == itr_ck.end());
        EXPECT_TRUE(prior(prior(itr_ck.end())) == itr_ck.begin());
        EXPECT_TRUE(*prior(itr_ck.end()) == std::string("foo"));
        EXPECT_TRUE(*prior(prior(itr_ck.end())) == std::string("c:"));

        itr_ck = path("c:/foo");
        EXPECT_TRUE(*itr_ck.begin() == std::string("c:"));
        EXPECT_TRUE(*next(itr_ck.begin()) == std::string("/"));
        EXPECT_TRUE(*next(next(itr_ck.begin())) == std::string("foo"));
        EXPECT_TRUE(next(next(next(itr_ck.begin()))) == itr_ck.end());
        EXPECT_TRUE(prior(prior(prior(itr_ck.end()))) == itr_ck.begin());
        EXPECT_TRUE(*prior(itr_ck.end()) == std::string("foo"));
        EXPECT_TRUE(*prior(prior(itr_ck.end())) == std::string("/"));
        EXPECT_TRUE(*prior(prior(prior(itr_ck.end()))) == std::string("c:"));

        itr_ck = path("//net");
        EXPECT_TRUE(*itr_ck.begin() == std::string("//net"));
        EXPECT_TRUE(next(itr_ck.begin()) == itr_ck.end());
        EXPECT_TRUE(prior(itr_ck.end()) == itr_ck.begin());
        EXPECT_TRUE(*prior(itr_ck.end()) == std::string("//net"));

        itr_ck = path("//net/");
        EXPECT_EQ(itr_ck.begin()->string(), "//net");
        EXPECT_EQ(next(itr_ck.begin())->string(), "/");
        EXPECT_TRUE(next(next(itr_ck.begin())) == itr_ck.end());
        EXPECT_TRUE(prior(prior(itr_ck.end())) == itr_ck.begin());
        EXPECT_EQ(prior(itr_ck.end())->string(), "/");
        EXPECT_EQ(prior(prior(itr_ck.end()))->string(), "//net");

        itr_ck = path("//net/foo");
        EXPECT_TRUE(*itr_ck.begin() == std::string("//net"));
        EXPECT_TRUE(*next(itr_ck.begin()) == std::string("/"));
        EXPECT_TRUE(*next(next(itr_ck.begin())) == std::string("foo"));
        EXPECT_TRUE(next(next(next(itr_ck.begin()))) == itr_ck.end());
        EXPECT_TRUE(prior(prior(prior(itr_ck.end()))) == itr_ck.begin());
        EXPECT_TRUE(*prior(itr_ck.end()) == std::string("foo"));
        EXPECT_TRUE(*prior(prior(itr_ck.end())) == std::string("/"));
        EXPECT_TRUE(*prior(prior(prior(itr_ck.end()))) == std::string("//net"));

        itr_ck = path("prn:");
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

TEST_F(PathTest, non_member_tests) {
    // test non-member functions, particularly operator overloads

    path e, e2;
    std::string es, es2;
    char ecs[] = "";
    char ecs2[] = "";

    char acs[] = "a";
    std::string as(acs);
    path a(as);

    char acs2[] = "a";
    std::string as2(acs2);
    path a2(as2);

    char bcs[] = "b";
    std::string bs(bcs);
    path b(bs);

    // swap
    a.swap(b);
    EXPECT_TRUE(a.string() == "b");
    EXPECT_TRUE(b.string() == "a");

    b.swap(a);
    EXPECT_TRUE(a.string() == "a");
    EXPECT_TRUE(b.string() == "b");

    // probe operator /
    EXPECT_EQ(path("") / ".", ".");
    EXPECT_EQ(path("") / "..", "..");

// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
        EXPECT_TRUE(path("foo\\bar") == "foo/bar");
        EXPECT_TRUE((b / a).native() == path("b\\a").native());
        EXPECT_TRUE((bs / a).native() == path("b\\a").native());
        EXPECT_TRUE((bcs / a).native() == path("b\\a").native());
        EXPECT_TRUE((b / as).native() == path("b\\a").native());
        EXPECT_TRUE((b / acs).native() == path("b\\a").native());
        EXPECT_EQ(path("a") / "b", "a\\b");
        EXPECT_EQ(path("..") / "", "..");
        EXPECT_EQ(path("foo") / path("bar"), "foo\\bar"); // path arg
        EXPECT_EQ(path("foo") / "bar", "foo\\bar");       // const char* arg
        EXPECT_EQ(path("foo") / path("woo/bar").filename(), "foo\\bar"); // const std::string & arg
        EXPECT_EQ("foo" / path("bar"), "foo\\bar");
        EXPECT_EQ(path("..") / ".." , "..\\..");
        EXPECT_EQ(path("/") / ".." , "/..");
        EXPECT_EQ(path("/..") / ".." , "/..\\..");
        EXPECT_EQ(path("..") / "foo" , "..\\foo");
        EXPECT_EQ(path("foo") / ".." , "foo\\..");
        EXPECT_EQ(path("..") / "f" , "..\\f");
        EXPECT_EQ(path("/..") / "f" , "/..\\f");
        EXPECT_EQ(path("f") / ".." , "f\\..");
        EXPECT_EQ(path("foo") / ".." / ".." , "foo\\..\\..");
        EXPECT_EQ(path("foo") / ".." / ".." / ".." , "foo\\..\\..\\..");
        EXPECT_EQ(path("f") / ".." / "b" , "f\\..\\b");
        EXPECT_EQ(path("foo") / ".." / "bar" , "foo\\..\\bar");
        EXPECT_EQ(path("foo") / "bar" / ".." , "foo\\bar\\..");
        EXPECT_EQ(path("foo") / "bar" / ".." / "..", "foo\\bar\\..\\..");
        EXPECT_EQ(path("foo") / "bar" / ".." / "blah", "foo\\bar\\..\\blah");
        EXPECT_EQ(path("f") / "b" / ".." , "f\\b\\..");
        EXPECT_EQ(path("f") / "b" / ".." / "a", "f\\b\\..\\a");
        EXPECT_EQ(path("foo") / "bar" / "blah" / ".." / "..", "foo\\bar\\blah\\..\\..");
        EXPECT_EQ(path("foo") / "bar" / "blah" / ".." / ".." / "bletch", "foo\\bar\\blah\\..\\..\\bletch");

        EXPECT_EQ(path(".") / "foo", ".\\foo");
        EXPECT_EQ(path(".") / "..", ".\\..");
        EXPECT_EQ(path("foo") / ".", "foo\\.");
        EXPECT_EQ(path("..") / ".", "..\\.");
        EXPECT_EQ(path(".") / ".", ".\\.");
        EXPECT_EQ(path(".") / "." / ".", ".\\.\\.");
        EXPECT_EQ(path(".") / "foo" / ".", ".\\foo\\.");
        EXPECT_EQ(path("foo") / "." / "bar", "foo\\.\\bar");
        EXPECT_EQ(path("foo") / "." / ".", "foo\\.\\.");
        EXPECT_EQ(path("foo") / "." / "..", "foo\\.\\..");
        EXPECT_EQ(path(".") / "." / "..", ".\\.\\..");
        EXPECT_EQ(path(".") / ".." / ".", ".\\..\\.");
        EXPECT_EQ(path("..") / "." / ".", "..\\.\\.");
#else
        EXPECT_EQ(b / a, "b/a");
        EXPECT_EQ(bs / a, "b/a");
        EXPECT_EQ(bcs / a, "b/a");
        EXPECT_EQ(b / as, "b/a");
        EXPECT_EQ(b / acs, "b/a");
        EXPECT_EQ(path("a") / "b", "a/b");
        EXPECT_EQ(path("..") / "", "..");
        EXPECT_EQ(path("") / "..", "..");
        EXPECT_EQ(path("foo") / path("bar"), "foo/bar"); // path arg
        EXPECT_EQ(path("foo") / "bar", "foo/bar");       // const char* arg
        EXPECT_EQ(path("foo") / path("woo/bar").filename(), "foo/bar"); // const std::string & arg
        EXPECT_EQ("foo" / path("bar"), "foo/bar");
        EXPECT_EQ(path("..") / ".." , "../..");
        EXPECT_EQ(path("/") / ".." , "/..");
        EXPECT_EQ(path("/..") / ".." , "/../..");
        EXPECT_EQ(path("..") / "foo" , "../foo");
        EXPECT_EQ(path("foo") / ".." , "foo/..");
        EXPECT_EQ(path("..") / "f" , "../f");
        EXPECT_EQ(path("/..") / "f" , "/../f");
        EXPECT_EQ(path("f") / ".." , "f/..");
        EXPECT_EQ(path("foo") / ".." / ".." , "foo/../..");
        EXPECT_EQ(path("foo") / ".." / ".." / ".." , "foo/../../..");
        EXPECT_EQ(path("f") / ".." / "b" , "f/../b");
        EXPECT_EQ(path("foo") / ".." / "bar" , "foo/../bar");
        EXPECT_EQ(path("foo") / "bar" / ".." , "foo/bar/..");
        EXPECT_EQ(path("foo") / "bar" / ".." / "..", "foo/bar/../..");
        EXPECT_EQ(path("foo") / "bar" / ".." / "blah", "foo/bar/../blah");
        EXPECT_EQ(path("f") / "b" / ".." , "f/b/..");
        EXPECT_EQ(path("f") / "b" / ".." / "a", "f/b/../a");
        EXPECT_EQ(path("foo") / "bar" / "blah" / ".." / "..", "foo/bar/blah/../..");
        EXPECT_EQ(path("foo") / "bar" / "blah" / ".." / ".." / "bletch", "foo/bar/blah/../../bletch");

        EXPECT_EQ(path(".") / "foo", "./foo");
        EXPECT_EQ(path(".") / "..", "./..");
        EXPECT_EQ(path("foo") / ".", "foo/.");
        EXPECT_EQ(path("..") / ".", "../.");
        EXPECT_EQ(path(".") / ".", "./.");
        EXPECT_EQ(path(".") / "." / ".", "././.");
        EXPECT_EQ(path(".") / "foo" / ".", "./foo/.");
        EXPECT_EQ(path("foo") / "." / "bar", "foo/./bar");
        EXPECT_EQ(path("foo") / "." / ".", "foo/./.");
        EXPECT_EQ(path("foo") / "." / "..", "foo/./..");
        EXPECT_EQ(path(".") / "." / "..", "././..");
        EXPECT_EQ(path(".") / ".." / ".", "./../.");
        EXPECT_EQ(path("..") / "." / ".", ".././.");
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

    // make sure basic_path overloads don't conflict with std::string overloads

    EXPECT_TRUE(!(as < as));
    EXPECT_TRUE(!(as < acs));
    EXPECT_TRUE(!(acs < as));

    // character set reality check before lexicographical tests
    EXPECT_TRUE(std::string("a.b") < std::string("a/b"));
    // verify compare is actually lexicographical
    EXPECT_TRUE(path("a/b") < path("a.b"));
    EXPECT_TRUE(path("a/b") == path("a///b"));
    EXPECT_TRUE(path("a/b/") == path("a/b/."));
    EXPECT_TRUE(path("a/b") != path("a/b/"));

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

    path p101("fe/fi/fo/fum");
    path p102(p101);
    path p103("fe/fi/fo/fumm");
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
        path p10 ("c:\\file");
        path p11 ("c:/file");
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

TEST_F(PathTest, query_and_decomposition_tests) {
    // these are the examples given in reference docs, so check they work
    EXPECT_TRUE(path("/foo/bar.txt").parent_path() == "/foo");
    EXPECT_TRUE(path("/foo/bar").parent_path() == "/foo");
    EXPECT_TRUE(path("/foo/bar/").parent_path() == "/foo/bar");
    EXPECT_TRUE(path("/").parent_path() == "");
    EXPECT_TRUE(path(".").parent_path() == "");
    EXPECT_TRUE(path("..").parent_path() == "");
    EXPECT_TRUE(path("/foo/bar.txt").filename() == "bar.txt");
    EXPECT_TRUE(path("/foo/bar").filename() == "bar");
    EXPECT_TRUE(path("/foo/bar/").filename() == ".");
    EXPECT_TRUE(path("/").filename() == "/");
    EXPECT_TRUE(path(".").filename() == ".");
    EXPECT_TRUE(path("..").filename() == "..");

    // stem() tests not otherwise covered
    EXPECT_TRUE(path(".").stem() == ".");
    EXPECT_TRUE(path("..").stem() == "..");
    EXPECT_TRUE(path(".a").stem() == "");
    EXPECT_TRUE(path("b").stem() == "b");
    EXPECT_TRUE(path("a/b.txt").stem() == "b");
    EXPECT_TRUE(path("a/b.").stem() == "b");
    EXPECT_TRUE(path("a.b.c").stem() == "a.b");
    EXPECT_TRUE(path("a.b.c.").stem() == "a.b.c");

    // extension() tests not otherwise covered
    EXPECT_TRUE(path(".").extension() == "");
    EXPECT_TRUE(path("..").extension() == "");
    EXPECT_TRUE(path(".a").extension() == ".a");
    EXPECT_TRUE(path("a/b").extension() == "");
    EXPECT_TRUE(path("a.b/c").extension() == "");
    EXPECT_TRUE(path("a/b.txt").extension() == ".txt");
    EXPECT_TRUE(path("a/b.").extension() == ".");
    EXPECT_TRUE(path("a.b.c").extension() == ".c");
    EXPECT_TRUE(path("a.b.c.").extension() == ".");
    EXPECT_TRUE(path("a/").extension() == "");

    // main q & d test sequence
    path p;
    path q;

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
    EXPECT_EQ(path("//resource"), "//resource");
    EXPECT_EQ(path("//resource/"), "//resource/");
    EXPECT_EQ(path("//resource/foo"), "//resource/foo");

    p = q = path("//net");
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

    p = q = path("//net/");
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

    p = q = path("//net/foo");
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

    p = q = path("//net///foo");
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
    p = path(":");
    EXPECT_EQ(p.parent_path().string(), "");
    EXPECT_EQ(p.filename(), ":");
    EXPECT_TRUE(!p.has_parent_path());
    EXPECT_TRUE(p.has_filename());

    //  test some similar cases that both POSIX and Windows should handle identically
    p = path("c:");
    EXPECT_EQ(p.parent_path().string(), "");
    EXPECT_EQ(p.filename(), "c:");
    EXPECT_TRUE(!p.has_parent_path());
    EXPECT_TRUE(p.has_filename());
    p = path("cc:");
    EXPECT_EQ(p.parent_path().string(), "");
    EXPECT_EQ(p.filename(), "cc:");
    EXPECT_TRUE(!p.has_parent_path());
    EXPECT_TRUE(p.has_filename());

    //  Windows specific tests
// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
      p = q = path("c:");
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

      p = q = path("c:foo");
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

      p = q = path("c:/");
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

      p = q = path("c:..");
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

      p = q = path("c:/foo");
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

      p = q = path("c://foo");
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

      p = q = path("c:\\foo\\bar");
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

      p = q = path("prn:");
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

      p = q = path("\\\\net\\\\\\foo");
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
      EXPECT_EQ(path("/foo/bar/"), "/foo/bar/");
      EXPECT_EQ(path("//foo//bar//"), "//foo//bar//");
      EXPECT_EQ(path("///foo///bar///"), "///foo///bar///");

      p = path("/usr/local/bin:/usr/bin:/bin");
      EXPECT_TRUE(p.string() == "/usr/local/bin:/usr/bin:/bin");
#endif
}

TEST_F(PathTest, construction_tests) {
    EXPECT_EQ(path(""), "");

    EXPECT_EQ(path("foo"), "foo");
    EXPECT_EQ(path("f"), "f");

    EXPECT_EQ(path("foo/"), "foo/");
    EXPECT_EQ(path("f/"), "f/");
    EXPECT_EQ(path("foo/.."), "foo/..");
    EXPECT_EQ(path("foo/../"), "foo/../");
    EXPECT_EQ(path("foo/bar/../.."), "foo/bar/../..");
    EXPECT_EQ(path("foo/bar/../../"), "foo/bar/../../");
    EXPECT_EQ(path("/"), "/");
    EXPECT_EQ(path("/f"), "/f");

    EXPECT_EQ(path("/foo"), "/foo");
    EXPECT_EQ(path("/foo/bar/"), "/foo/bar/");
    EXPECT_EQ(path("//foo//bar//"), "//foo//bar//");
    EXPECT_EQ(path("///foo///bar///"), "///foo///bar///");
    EXPECT_EQ(path("\\/foo\\/bar\\/"), "\\/foo\\/bar\\/");
    EXPECT_EQ(path("\\//foo\\//bar\\//"), "\\//foo\\//bar\\//");

// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
      EXPECT_EQ(path("c:") / "foo", "c:foo");
      EXPECT_EQ(path("c:") / "/foo", "c:/foo");

      EXPECT_EQ(path("\\foo\\bar\\"), "\\foo\\bar\\");
      EXPECT_EQ(path("\\\\foo\\\\bar\\\\"), "\\\\foo\\\\bar\\\\");
      EXPECT_EQ(path("\\\\\\foo\\\\\\bar\\\\\\"), "\\\\\\foo\\\\\\bar\\\\\\");

      EXPECT_EQ(path("\\"), "\\");
      EXPECT_EQ(path("\\f"), "\\f");
      EXPECT_EQ(path("\\foo"), "\\foo");
      EXPECT_EQ(path("foo\\bar"), "foo\\bar");
      EXPECT_EQ(path("foo bar"), "foo bar");
      EXPECT_EQ(path("c:"), "c:");
      EXPECT_EQ(path("c:/"), "c:/");
      EXPECT_EQ(path("c:."), "c:.");
      EXPECT_EQ(path("c:./foo"), "c:./foo");
      EXPECT_EQ(path("c:.\\foo"), "c:.\\foo");
      EXPECT_EQ(path("c:.."), "c:..");
      EXPECT_EQ(path("c:/."), "c:/.");
      EXPECT_EQ(path("c:/.."), "c:/..");
      EXPECT_EQ(path("c:/../"), "c:/../");
      EXPECT_EQ(path("c:\\..\\"), "c:\\..\\");
      EXPECT_EQ(path("c:/../.."), "c:/../..");
      EXPECT_EQ(path("c:/../foo"), "c:/../foo");
      EXPECT_EQ(path("c:\\..\\foo"), "c:\\..\\foo");
      EXPECT_EQ(path("c:../foo"), "c:../foo");
      EXPECT_EQ(path("c:..\\foo"), "c:..\\foo");
      EXPECT_EQ(path("c:/../../foo"), "c:/../../foo");
      EXPECT_EQ(path("c:\\..\\..\\foo"), "c:\\..\\..\\foo");
      EXPECT_EQ(path("c:foo/.."), "c:foo/..");
      EXPECT_EQ(path("c:/foo/.."), "c:/foo/..");
      EXPECT_EQ(path("c:/..foo"), "c:/..foo");
      EXPECT_EQ(path("c:foo"), "c:foo");
      EXPECT_EQ(path("c:/foo"), "c:/foo");
      EXPECT_EQ(path("\\\\netname"), "\\\\netname");
      EXPECT_EQ(path("\\\\netname\\"), "\\\\netname\\");
      EXPECT_EQ(path("\\\\netname\\foo"), "\\\\netname\\foo");
      EXPECT_EQ(path("c:/foo"), "c:/foo");
      EXPECT_EQ(path("prn:"), "prn:");
#endif

    EXPECT_EQ(path("foo/bar"), "foo/bar");
    EXPECT_EQ(path("a/b"), "a/b");  // probe for length effects
    EXPECT_EQ(path(".."), "..");
    EXPECT_EQ(path("../.."), "../..");
    EXPECT_EQ(path("/.."), "/..");
    EXPECT_EQ(path("/../.."), "/../..");
    EXPECT_EQ(path("../foo"), "../foo");
    EXPECT_EQ(path("foo/.."), "foo/..");
    EXPECT_EQ(path("foo/..bar"), "foo/..bar");
    EXPECT_EQ(path("../f"), "../f");
    EXPECT_EQ(path("/../f"), "/../f");
    EXPECT_EQ(path("f/.."), "f/..");
    EXPECT_EQ(path("foo/../.."), "foo/../..");
    EXPECT_EQ(path("foo/../../.."), "foo/../../..");
    EXPECT_EQ(path("foo/../bar"), "foo/../bar");
    EXPECT_EQ(path("foo/bar/.."), "foo/bar/..");
    EXPECT_EQ(path("foo/bar/../.."), "foo/bar/../..");
    EXPECT_EQ(path("foo/bar/../blah"), "foo/bar/../blah");
    EXPECT_EQ(path("f/../b"), "f/../b");
    EXPECT_EQ(path("f/b/.."), "f/b/..");
    EXPECT_EQ(path("f/b/../a"), "f/b/../a");
    EXPECT_EQ(path("foo/bar/blah/../.."), "foo/bar/blah/../..");
    EXPECT_EQ(path("foo/bar/blah/../../bletch"), "foo/bar/blah/../../bletch");
    EXPECT_EQ(path("..."), "...");
    EXPECT_EQ(path("...."), "....");
    EXPECT_EQ(path("foo/..."), "foo/...");
    EXPECT_EQ(path("abc."), "abc.");
    EXPECT_EQ(path("abc.."), "abc..");
    EXPECT_EQ(path("foo/abc."), "foo/abc.");
    EXPECT_EQ(path("foo/abc.."), "foo/abc..");

    EXPECT_EQ(path(".abc"), ".abc");
    EXPECT_EQ(path("a.c"), "a.c");
    EXPECT_EQ(path("..abc"), "..abc");
    EXPECT_EQ(path("a..c"), "a..c");
    EXPECT_EQ(path("foo/.abc"), "foo/.abc");
    EXPECT_EQ(path("foo/a.c"), "foo/a.c");
    EXPECT_EQ(path("foo/..abc"), "foo/..abc");
    EXPECT_EQ(path("foo/a..c"), "foo/a..c");

    EXPECT_EQ(path("."), ".");
    EXPECT_EQ(path("./foo"), "./foo");
    EXPECT_EQ(path("./.."), "./..");
    EXPECT_EQ(path("./../foo"), "./../foo");
    EXPECT_EQ(path("foo/."), "foo/.");
    EXPECT_EQ(path("../."), "../.");
    EXPECT_EQ(path("./."), "./.");
    EXPECT_EQ(path("././."), "././.");
    EXPECT_EQ(path("./foo/."), "./foo/.");
    EXPECT_EQ(path("foo/./bar"), "foo/./bar");
    EXPECT_EQ(path("foo/./."), "foo/./.");
    EXPECT_EQ(path("foo/./.."), "foo/./..");
    EXPECT_EQ(path("foo/./../bar"), "foo/./../bar");
    EXPECT_EQ(path("foo/../."), "foo/../.");
    EXPECT_EQ(path("././.."), "././..");
    EXPECT_EQ(path("./../."), "./../.");
    EXPECT_EQ(path(".././."), ".././.");
}

namespace {

void append_test_aux(const path& p, const std::string& s, const std::string& expect) {
    EXPECT_EQ((p / path(s)).string(), expect);
    EXPECT_EQ((p / s.c_str()).string(), expect);
    EXPECT_EQ((p / s).string(), expect);
    path x(p);
    x.append(s.begin(), s.end());
    EXPECT_EQ(x.string(), expect);
}

} // namespace

TEST_F(PathTest, append_tests) {
    EXPECT_EQ(path("") / "", "");
    append_test_aux("", "", "");

    EXPECT_EQ(path("") / "/", "/");
    append_test_aux("", "/", "/");

    EXPECT_EQ(path("") / "bar", "bar");
    append_test_aux("", "bar", "bar");

    EXPECT_EQ(path("") / "/bar", "/bar");
    append_test_aux("", "/bar", "/bar");

    EXPECT_EQ(path("/") / "", "/");
    append_test_aux("/", "", "/");

    EXPECT_EQ(path("/") / "/", "//");
    append_test_aux("/", "/", "//");

    EXPECT_EQ(path("/") / "bar", "/bar");
    append_test_aux("/", "bar", "/bar");

    EXPECT_EQ(path("/") / "/bar", "//bar");
    append_test_aux("/", "/bar", "//bar");

    EXPECT_EQ(path("foo") / "", "foo");
    append_test_aux("foo", "", "foo");

    EXPECT_EQ(path("foo") / "/", "foo/");
    append_test_aux("foo", "/", "foo/");

    EXPECT_EQ(path("foo") / "/bar", "foo/bar");
    append_test_aux("foo", "/bar", "foo/bar");

    EXPECT_EQ(path("foo/") / "", "foo/");
    append_test_aux("foo/", "", "foo/");

    EXPECT_EQ(path("foo/") / "/", "foo//");
    append_test_aux("foo/", "/", "foo//");

    EXPECT_EQ(path("foo/") / "bar", "foo/bar");
    append_test_aux("foo/", "bar", "foo/bar");


// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
      EXPECT_EQ(path("foo") / "bar", "foo\\bar");
      append_test_aux("foo", "bar", "foo\\bar");

      EXPECT_EQ(path("foo\\") / "\\bar", "foo\\\\bar");
      append_test_aux("foo\\", "\\bar", "foo\\\\bar");

      // hand created test case specific to Windows
      EXPECT_EQ(path("c:") / "bar", "c:bar");
      append_test_aux("c:", "bar", "c:bar");
#else
      EXPECT_EQ(path("foo") / "bar", "foo/bar");
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

    path p6819;
    p6819 /= u.a;
    EXPECT_EQ(p6819, path("ab"));
}

TEST_F(PathTest, self_assign_and_append_tests) {
    path p;

    p = "snafubar";
    EXPECT_EQ(p = p, "snafubar");

    p = "snafubar";
    p = p.c_str();
    EXPECT_EQ(p, "snafubar");

    p = "snafubar";
    p.assign(p.c_str(), path::codecvt());
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
    p.append(p.c_str(), path::codecvt());
    EXPECT_EQ(p, "snafubar" BOOST_DIR_SEP "snafubar");

    p = "snafubar";
    EXPECT_EQ(p.append(p.c_str() + 5, p.c_str() + 7), "snafubar" BOOST_DIR_SEP "ba");
}

// TODO: skipped void name_function_tests() from original source code
// need to return it???

TEST_F(PathTest, replace_extension_tests) {
    EXPECT_TRUE(path().replace_extension().empty());
    EXPECT_TRUE(path().replace_extension("a") == ".a");
    EXPECT_TRUE(path().replace_extension("a.") == ".a.");
    EXPECT_TRUE(path().replace_extension(".a") == ".a");
    EXPECT_TRUE(path().replace_extension("a.txt") == ".a.txt");
    // see the rationale in html docs for explanation why this works:
    EXPECT_TRUE(path().replace_extension(".txt") == ".txt");

    EXPECT_TRUE(path("a.txt").replace_extension() == "a");
    EXPECT_TRUE(path("a.txt").replace_extension("") == "a");
    EXPECT_TRUE(path("a.txt").replace_extension(".") == "a.");
    EXPECT_TRUE(path("a.txt").replace_extension(".tex") == "a.tex");
    EXPECT_TRUE(path("a.txt").replace_extension("tex") == "a.tex");
    EXPECT_TRUE(path("a.").replace_extension(".tex") == "a.tex");
    EXPECT_TRUE(path("a.").replace_extension("tex") == "a.tex");
    EXPECT_TRUE(path("a").replace_extension(".txt") == "a.txt");
    EXPECT_TRUE(path("a").replace_extension("txt") == "a.txt");
    EXPECT_TRUE(path("a.b.txt").replace_extension(".tex") == "a.b.tex");
    EXPECT_TRUE(path("a.b.txt").replace_extension("tex") == "a.b.tex");
    EXPECT_TRUE(path("a/b").replace_extension(".c") == "a/b.c");
    EXPECT_EQ(path("a.txt/b").replace_extension(".c"), "a.txt/b.c");    // ticket 4702
    EXPECT_TRUE(path("foo.txt").replace_extension("exe") == "foo.exe"); // ticket 5118
    EXPECT_TRUE(path("foo.txt").replace_extension(".tar.bz2")
                                                    == "foo.tar.bz2");  // ticket 5118
  }

TEST_F(PathTest, make_preferred_tests) {
// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
    EXPECT_TRUE(path("//abc\\def/ghi").make_preferred().native() == path("\\\\abc\\def\\ghi").native());
#else
    EXPECT_TRUE(path("//abc\\def/ghi").make_preferred().native() == path("//abc\\def/ghi").native());
#endif
}

TEST_F(PathTest, lexically_normal_tests) {
    //  Note: lexically_lexically_normal() uses /= to build up some results, so these results will
    //  have the platform's preferred separator. Since that is immaterial to the correct
    //  functioning of lexically_lexically_normal(), the test results are converted to generic form,
    //  and the expected results are also given in generic form. Otherwise many of the
    //  tests would incorrectly be reported as failing on Windows.

    EXPECT_EQ(path("").lexically_normal().generic_path(), "");
    EXPECT_EQ(path("/").lexically_normal().generic_path(), "/");
    EXPECT_EQ(path("//").lexically_normal().generic_path(), "//");
    EXPECT_EQ(path("///").lexically_normal().generic_path(), "/");
    EXPECT_EQ(path("f").lexically_normal().generic_path(), "f");
    EXPECT_EQ(path("foo").lexically_normal().generic_path(), "foo");
    EXPECT_EQ(path("foo/").lexically_normal().generic_path(), "foo/.");
    EXPECT_EQ(path("f/").lexically_normal().generic_path(), "f/.");
    EXPECT_EQ(path("/foo").lexically_normal().generic_path(), "/foo");
    EXPECT_EQ(path("foo/bar").lexically_normal().generic_path(), "foo/bar");
    EXPECT_EQ(path("..").lexically_normal().generic_path(), "..");
    EXPECT_EQ(path("../..").lexically_normal().generic_path(), "../..");
    EXPECT_EQ(path("/..").lexically_normal().generic_path(), "/..");
    EXPECT_EQ(path("/../..").lexically_normal().generic_path(), "/../..");
    EXPECT_EQ(path("../foo").lexically_normal().generic_path(), "../foo");
    EXPECT_EQ(path("foo/..").lexically_normal().generic_path(), ".");
    EXPECT_EQ(path("foo/../").lexically_normal().generic_path(), "./.");
    EXPECT_EQ((path("foo") / "..").lexically_normal().generic_path() , ".");
    EXPECT_EQ(path("foo/...").lexically_normal().generic_path(), "foo/...");
    EXPECT_EQ(path("foo/.../").lexically_normal().generic_path(), "foo/.../.");
    EXPECT_EQ(path("foo/..bar").lexically_normal().generic_path(), "foo/..bar");
    EXPECT_EQ(path("../f").lexically_normal().generic_path(), "../f");
    EXPECT_EQ(path("/../f").lexically_normal().generic_path(), "/../f");
    EXPECT_EQ(path("f/..").lexically_normal().generic_path(), ".");
    EXPECT_EQ((path("f") / "..").lexically_normal().generic_path() , ".");
    EXPECT_EQ(path("foo/../..").lexically_normal().generic_path(), "..");
    EXPECT_EQ(path("foo/../../").lexically_normal().generic_path(), "../.");
    EXPECT_EQ(path("foo/../../..").lexically_normal().generic_path(), "../..");
    EXPECT_EQ(path("foo/../../../").lexically_normal().generic_path(), "../../.");
    EXPECT_EQ(path("foo/../bar").lexically_normal().generic_path(), "bar");
    EXPECT_EQ(path("foo/../bar/").lexically_normal().generic_path(), "bar/.");
    EXPECT_EQ(path("foo/bar/..").lexically_normal().generic_path(), "foo");
    EXPECT_EQ(path("foo/./bar/..").lexically_normal().generic_path(), "foo");
    EXPECT_EQ(path("foo/bar/../").lexically_normal().generic_path(), "foo/.");
    EXPECT_EQ(path("foo/./bar/../").lexically_normal().generic_path(), "foo/.");
    EXPECT_EQ(path("foo/bar/../..").lexically_normal().generic_path(), ".");
    EXPECT_EQ(path("foo/bar/../../").lexically_normal().generic_path(), "./.");
    EXPECT_EQ(path("foo/bar/../blah").lexically_normal().generic_path(), "foo/blah");
    EXPECT_EQ(path("f/../b").lexically_normal().generic_path(), "b");
    EXPECT_EQ(path("f/b/..").lexically_normal().generic_path(), "f");
    EXPECT_EQ(path("f/b/../").lexically_normal().generic_path(), "f/.");
    EXPECT_EQ(path("f/b/../a").lexically_normal().generic_path(), "f/a");
    EXPECT_EQ(path("foo/bar/blah/../..").lexically_normal().generic_path(), "foo");
    EXPECT_EQ(path("foo/bar/blah/../../bletch").lexically_normal().generic_path(), "foo/bletch");
    EXPECT_EQ(path("//net").lexically_normal().generic_path(), "//net");
    EXPECT_EQ(path("//net/").lexically_normal().generic_path(), "//net/");
    EXPECT_EQ(path("//..net").lexically_normal().generic_path(), "//..net");
    EXPECT_EQ(path("//net/..").lexically_normal().generic_path(), "//net/..");
    EXPECT_EQ(path("//net/foo").lexically_normal().generic_path(), "//net/foo");
    EXPECT_EQ(path("//net/foo/").lexically_normal().generic_path(), "//net/foo/.");
    EXPECT_EQ(path("//net/foo/..").lexically_normal().generic_path(), "//net/");
    EXPECT_EQ(path("//net/foo/../").lexically_normal().generic_path(), "//net/.");

    EXPECT_EQ(path("/net/foo/bar").lexically_normal().generic_path(), "/net/foo/bar");
    EXPECT_EQ(path("/net/foo/bar/").lexically_normal().generic_path(), "/net/foo/bar/.");
    EXPECT_EQ(path("/net/foo/..").lexically_normal().generic_path(), "/net");
    EXPECT_EQ(path("/net/foo/../").lexically_normal().generic_path(), "/net/.");

    EXPECT_EQ(path("//net//foo//bar").lexically_normal().generic_path(), "//net/foo/bar");
    EXPECT_EQ(path("//net//foo//bar//").lexically_normal().generic_path(), "//net/foo/bar/.");
    EXPECT_EQ(path("//net//foo//..").lexically_normal().generic_path(), "//net/");
    EXPECT_EQ(path("//net//foo//..//").lexically_normal().generic_path(), "//net/.");

    EXPECT_EQ(path("///net///foo///bar").lexically_normal().generic_path(), "/net/foo/bar");
    EXPECT_EQ(path("///net///foo///bar///").lexically_normal().generic_path(), "/net/foo/bar/.");
    EXPECT_EQ(path("///net///foo///..").lexically_normal().generic_path(), "/net");
    EXPECT_EQ(path("///net///foo///..///").lexically_normal().generic_path(), "/net/.");

// TODO: fixme
#ifdef BUILD_FOR_WINDOWS
      EXPECT_EQ(path("c:..").lexically_normal().generic_path(), "c:..");
      EXPECT_EQ(path("c:foo/..").lexically_normal().generic_path(), "c:");

      EXPECT_EQ(path("c:foo/../").lexically_normal().generic_path(), "c:.");

      EXPECT_EQ(path("c:/foo/..").lexically_normal().generic_path(), "c:/");
      EXPECT_EQ(path("c:/foo/../").lexically_normal().generic_path(), "c:/.");
      EXPECT_EQ(path("c:/..").lexically_normal().generic_path(), "c:/..");
      EXPECT_EQ(path("c:/../").lexically_normal().generic_path(), "c:/../.");
      EXPECT_EQ(path("c:/../..").lexically_normal().generic_path(), "c:/../..");
      EXPECT_EQ(path("c:/../../").lexically_normal().generic_path(), "c:/../../.");
      EXPECT_EQ(path("c:/../foo").lexically_normal().generic_path(), "c:/../foo");
      EXPECT_EQ(path("c:/../foo/").lexically_normal().generic_path(), "c:/../foo/.");
      EXPECT_EQ(path("c:/../../foo").lexically_normal().generic_path(), "c:/../../foo");
      EXPECT_EQ(path("c:/../../foo/").lexically_normal().generic_path(), "c:/../../foo/.");
      EXPECT_EQ(path("c:/..foo").lexically_normal().generic_path(), "c:/..foo");
#else
      EXPECT_EQ(path("c:..").lexically_normal(), "c:..");
      EXPECT_EQ(path("c:foo/..").lexically_normal(), ".");
      EXPECT_EQ(path("c:foo/../").lexically_normal(), "./.");
      EXPECT_EQ(path("c:/foo/..").lexically_normal(), "c:");
      EXPECT_EQ(path("c:/foo/../").lexically_normal(), "c:/.");
      EXPECT_EQ(path("c:/..").lexically_normal(), ".");
      EXPECT_EQ(path("c:/../").lexically_normal(), "./.");
      EXPECT_EQ(path("c:/../..").lexically_normal(), "..");
      EXPECT_EQ(path("c:/../../").lexically_normal(), "../.");
      EXPECT_EQ(path("c:/../foo").lexically_normal(), "foo");
      EXPECT_EQ(path("c:/../foo/").lexically_normal(), "foo/.");
      EXPECT_EQ(path("c:/../../foo").lexically_normal(), "../foo");
      EXPECT_EQ(path("c:/../../foo/").lexically_normal(), "../foo/.");
      EXPECT_EQ(path("c:/..foo").lexically_normal(), "c:/..foo");
#endif
}

TEST_F(PathTest, lexically_relative_test) {
    EXPECT_TRUE(path("").lexically_relative("") == "");
    EXPECT_TRUE(path("").lexically_relative("/foo") == "");
    EXPECT_TRUE(path("/foo").lexically_relative("") == "");
    EXPECT_TRUE(path("/foo").lexically_relative("/foo") == ".");
    EXPECT_TRUE(path("").lexically_relative("foo") == "");
    EXPECT_TRUE(path("foo").lexically_relative("") == "");
    EXPECT_TRUE(path("foo").lexically_relative("foo") == ".");

    EXPECT_TRUE(path("a/b/c").lexically_relative("a") == "b/c");
    EXPECT_TRUE(path("a//b//c").lexically_relative("a") == "b/c");
    EXPECT_TRUE(path("a/b/c").lexically_relative("a/b") == "c");
    EXPECT_TRUE(path("a///b//c").lexically_relative("a//b") == "c");
    EXPECT_TRUE(path("a/b/c").lexically_relative("a/b/c") == ".");
    EXPECT_TRUE(path("a/b/c").lexically_relative("a/b/c/x") == "..");
    EXPECT_TRUE(path("a/b/c").lexically_relative("a/b/c/x/y") == "../..");
    EXPECT_TRUE(path("a/b/c").lexically_relative("a/x") == "../b/c");
    EXPECT_TRUE(path("a/b/c").lexically_relative("a/b/x") == "../c");
    EXPECT_TRUE(path("a/b/c").lexically_relative("a/x/y") == "../../b/c");
    EXPECT_TRUE(path("a/b/c").lexically_relative("a/b/x/y") == "../../c");
    EXPECT_TRUE(path("a/b/c").lexically_relative("a/b/c/x/y/z") == "../../..");

    // paths unrelated except first element, and first element is root directory
    EXPECT_TRUE(path("/a/b/c").lexically_relative("/x") == "../a/b/c");
    EXPECT_TRUE(path("/a/b/c").lexically_relative("/x/y") == "../../a/b/c");
    EXPECT_TRUE(path("/a/b/c").lexically_relative("/x/y/z") == "../../../a/b/c");

    // paths unrelated
    EXPECT_TRUE(path("a/b/c").lexically_relative("x") == "");
    EXPECT_TRUE(path("a/b/c").lexically_relative("x/y") == "");
    EXPECT_TRUE(path("a/b/c").lexically_relative("x/y/z") == "");
    EXPECT_TRUE(path("a/b/c").lexically_relative("/x") == "");
    EXPECT_TRUE(path("a/b/c").lexically_relative("/x/y") == "");
    EXPECT_TRUE(path("a/b/c").lexically_relative("/x/y/z") == "");
    EXPECT_TRUE(path("a/b/c").lexically_relative("/a/b/c") == "");

    // TODO: add some Windows-only test cases that probe presence or absence of
    // drive specifier-and root-directory

    //  Some tests from Jamie Allsop's paper
    EXPECT_TRUE(path("/a/d").lexically_relative("/a/b/c") == "../../d");
    EXPECT_TRUE(path("/a/b/c").lexically_relative("/a/d") == "../b/c");

// TODO: fixme
  #ifdef BOOST_WINDOWS_API
    EXPECT_TRUE(path("c:\\y").lexically_relative("c:\\x") == "../y");
  #else
    EXPECT_TRUE(path("c:\\y").lexically_relative("c:\\x") == "");
  #endif

    EXPECT_TRUE(path("d:\\y").lexically_relative("c:\\x") == "");

    //  From issue #1976
    EXPECT_TRUE(path("/foo/new").lexically_relative("/foo/bar") == "../new");
  }

TEST_F(PathTest, lexically_proximate_test) {
    // paths unrelated
    EXPECT_TRUE(path("a/b/c").lexically_proximate("x") == "a/b/c");
}

namespace {

inline void odr_use(const path::value_type& c) {
    static const path::value_type dummy = '\0';
    EXPECT_TRUE(&c != &dummy);
}

} // namespace

TEST_F(PathTest, odr_use) {
    odr_use(path::separator);
    odr_use(path::preferred_separator);
    odr_use(path::dot);
}

namespace {

path p1("fe/fi/fo/fum");
path p2(p1);
path p3;
path p4("foobar");
path p5;

} // namespace

TEST_F(PathTest, misc_test) {
  EXPECT_TRUE(p1.string() != p3.string());
  p3 = p2;
  EXPECT_TRUE(p1.string() == p3.string());

  path p04("foobar");
  EXPECT_TRUE(p04.string() == "foobar");
  p04 = p04; // self-assignment
  EXPECT_TRUE(p04.string() == "foobar");

  std::string s1("//:somestring");  // this used to be treated specially

  // check the path member templates
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
  EXPECT_TRUE(path("foo").filename() == "foo");
  EXPECT_TRUE(path("foo").parent_path().string() == "");
  EXPECT_TRUE(p1.filename() == "fum");
  EXPECT_TRUE(p1.parent_path().string() == "fe/fi/fo");
  EXPECT_TRUE(path("").empty() == true);
  EXPECT_TRUE(path("foo").empty() == false);
}
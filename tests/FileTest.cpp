#include "UTCommon.h"

#include "io/File.h"
#include "io/Dir.h"

#include <boost/filesystem.hpp>

namespace {

std::string create_temp_test_directory(const std::string& name_template) {
    const std::string tmp_dir_path = io::make_temp_dir(name_template);
    if (tmp_dir_path.empty()) {
        assert(false);
        return "";
    }

    return tmp_dir_path;
}

std::string create_empty_file(const std::string& path_where_create) {
    std::string file_path = path_where_create + "/empty";
    std::ofstream ofile(file_path);
    if (ofile.fail()) {
        return "";
    }
    ofile.close();

    return file_path;
}

} // namespace

struct FileTest : public testing::Test {
    FileTest() {
        auto tmp_path = boost::filesystem::temp_directory_path() / "uv_cpp";
        boost::filesystem::create_directories(tmp_path);
        m_tmp_test_dir = create_temp_test_directory((tmp_path / "XXXXXX").string());
    }

protected:
    std::string m_tmp_test_dir;
};

TEST_F(FileTest, default_constructor) {
    io::EventLoop loop;

    auto file = new io::File(loop);
    ASSERT_FALSE(file->is_open());
    file->schedule_removal();
}

TEST_F(FileTest, open_existing) {
    auto path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    bool opened = false;

    auto file = new io::File(loop);
    file->open(path, [&](io::File& file) {
        opened = true;
        file.schedule_removal();
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(opened);
}

TEST_F(FileTest, double_open) {
    auto path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    bool opened_1 = false;
    bool opened_2 = false;

    auto file = new io::File(loop);
    file->open(path, [&](io::File& file) {
        opened_1 = true;
    });

    // Overwriting callback and state, previous one will never be executed
    file->open(path, [&](io::File& file) {
        opened_2 = true;
        file.schedule_removal();
    });

    ASSERT_EQ(0, loop.run());

    EXPECT_FALSE(opened_1);
    EXPECT_TRUE(opened_2);

}

TEST_F(FileTest, open_in_open_callback) {
    auto path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    bool opened_1 = false;
    bool opened_2 = false;

    auto file = new io::File(loop);
    file->open(path, [&](io::File& file) {
        opened_1 = true;

        file.open(path, [&](io::File& file) {
            opened_2 = true;
            file.schedule_removal();
        });
    });

    ASSERT_EQ(0, loop.run());

    EXPECT_TRUE(opened_1);
    EXPECT_TRUE(opened_2);

}

TEST_F(FileTest, open_not_existing) {

}

TEST_F(FileTest, open_existing_open_not_existing) {

}

TEST_F(FileTest, close_in_open_callback) {

}

TEST_F(FileTest, close_not_open_file) {

}

TEST_F(FileTest, double_close) {

}

#include "UTCommon.h"

#include "io/File.h"
#include "io/Dir.h"

#include <boost/filesystem.hpp>

namespace {

std::string create_file_for_open(const std::string& name_template) {
    io::EventLoop loop;
    const std::string tmp_dir_path = io::make_temp_dir(loop, name_template);
    if (tmp_dir_path.empty()) {
        return "";
    }

    std::string file_path = tmp_dir_path + "/test";
    std::ofstream ofile(file_path);
    if (ofile.fail()) {
        return "";
    }
    ofile.close();

    return file_path;
}

}

struct FileTest : public testing::Test {
    FileTest() :
        m_tmp_path_template((boost::filesystem::temp_directory_path() / "XXXXXX").string()),
        m_open_file_path(create_file_for_open(m_tmp_path_template)) {

    }

protected:
    const std::string m_tmp_path_template;
    const std::string m_open_file_path;
};

TEST_F(FileTest, default_constructor) {
    io::EventLoop loop;

    auto file = new io::File(loop);
    ASSERT_FALSE(file->is_open());
    file->schedule_removal();
}

TEST_F(FileTest, open_existing) {
    io::EventLoop loop;

    bool opened = false;

    auto file = new io::File(loop);
    file->open(m_open_file_path, [&](io::File& file) {
        opened = true;
        file.schedule_removal();
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(opened);
}

TEST_F(FileTest, double_open) {

}

TEST_F(FileTest, open_not_existing) {

}

TEST_F(FileTest, close_in_open_callback) {

}

TEST_F(FileTest, close_not_open_file) {

}

TEST_F(FileTest, double_close) {

}
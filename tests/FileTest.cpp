#include "UTCommon.h"

#include "io/File.h"

struct FileTest : public testing::Test {

};

TEST_F(FileTest, default_constructor) {
    io::EventLoop loop;

    auto file = new io::File(loop);
    ASSERT_FALSE(file->is_open());
    file->schedule_removal();
}
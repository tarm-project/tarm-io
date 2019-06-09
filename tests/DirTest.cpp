
#include "UTCommon.h"

#include "io/Dir.h"

#include <thread>
#include <mutex>

struct DirTest : public testing::Test {

};

TEST_F(DirTest, default_constructor) {
    io::EventLoop loop;

    auto dir = new io::Dir(loop);

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();
}

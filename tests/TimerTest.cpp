#include "UTCommon.h"

#include "io/Timer.h"

struct TimerTest : public testing::Test {

};

TEST_F(TimerTest, default_constructor) {
    io::EventLoop loop;

    io::Timer timer(loop);

    ASSERT_EQ(0, loop.run());
}
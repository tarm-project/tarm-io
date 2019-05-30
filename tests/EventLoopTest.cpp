#include "UTCommon.h"

#include "io/EventLoop.h"

struct EventLoopTest : public testing::Test {

};

TEST_F(EventLoopTest, default_constructor) {
    io::EventLoop event_loop;
    ASSERT_EQ(0, event_loop.run());
}
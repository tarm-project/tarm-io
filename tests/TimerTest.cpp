#include "UTCommon.h"

#include "io/Timer.h"

#include <chrono>

struct TimerTest : public testing::Test {

};

TEST_F(TimerTest, default_constructor) {
    io::EventLoop loop;

    io::Timer timer(loop);

    ASSERT_EQ(0, loop.run());
}

TEST_F(TimerTest, no_repeat) {
    io::EventLoop loop;

    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time = start_time;

    const uint64_t TIMEOUT_MS = 100;
    size_t call_counter = 0;

    io::Timer timer(loop);
    timer.start(TIMEOUT_MS, 0, [&](io::Timer& timer) {
        end_time = std::chrono::high_resolution_clock::now();
        ++call_counter;
    });

    ASSERT_EQ(0, loop.run());

    const auto timer_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    EXPECT_GE(TIMEOUT_MS, timer_duration);

    EXPECT_EQ(1, call_counter);
}
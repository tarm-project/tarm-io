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
    EXPECT_GE(timer_duration, TIMEOUT_MS);

    EXPECT_EQ(1, call_counter);
}

TEST_F(TimerTest, zero_timeot) {
    // Note: in this test we check that timer is called on next loop cycle

    io::EventLoop loop;

    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time = start_time;

    std::size_t timer_counter = 0;
    std::size_t idle_counter = 0;

    std::size_t idle_handle = 0;
    idle_handle = loop.schedule_call_on_each_loop_cycle([&]() {
        ASSERT_EQ(1, timer_counter);
        ++idle_counter;

        loop.stop_call_on_each_loop_cycle(idle_handle);
    });

    io::Timer timer(loop);
    timer.start(0, 0, [&](io::Timer& timer) {
        ASSERT_EQ(0, idle_counter);

        end_time = std::chrono::high_resolution_clock::now();
        ++timer_counter;
    });

    ASSERT_EQ(0, loop.run());

    const auto timer_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    EXPECT_LE(timer_duration, 100);

    EXPECT_EQ(1, timer_counter);
}

#include "UTCommon.h"

#include "io/Timer.h"

#include <chrono>

struct TimerTest : public testing::Test,
                   public LogRedirector {

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
    EXPECT_GE(timer_duration, TIMEOUT_MS - 10); // -10 because sometime timer may wake up a bit earlier

    EXPECT_EQ(1, call_counter);
}

TEST_F(TimerTest, zero_timeot) {
    // Note: in this test we check that timer is called on next loop cycle

    io::EventLoop loop;

    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time = start_time;

    std::size_t timer_counter = 0;
    std::size_t idle_counter = 0;

    std::size_t idle_handle = io::EventLoop::INVALID_HANDLE;
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

TEST_F(TimerTest, no_callback) {
    // Note: this case handles some sort of idle to prevent loop from exit during desired time
    io::EventLoop loop;

    io::Timer timer(loop);
    timer.start(100, nullptr);

    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time = start_time;

    ASSERT_EQ(0, loop.run());
    const auto timer_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    EXPECT_LE(timer_duration, 100);
}

TEST_F(TimerTest, stop_after_start) {
    io::EventLoop loop;

    bool callback_called = false;

    io::Timer timer(loop);
    timer.start(100, [&](io::Timer& ) {
        callback_called = true;
    });
    timer.stop();

    ASSERT_EQ(0, loop.run());
    EXPECT_FALSE(callback_called);
}

TEST_F(TimerTest, stop_in_callback) {
    io::EventLoop loop;

    std::size_t calls_counter = 0;

    io::Timer timer(loop);
    timer.start(100, 100, [&](io::Timer& timer) {
        ++calls_counter;
        timer.stop();
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(1, calls_counter);
}

TEST_F(TimerTest, start_stop_start_stop) {
    io::EventLoop loop;

    bool first_callback_called = false;
    bool second_callback_called = false;

    io::Timer timer(loop);
    timer.start(100, 100, [&](io::Timer& timer) {
        first_callback_called = true;
    });

    timer.stop();

    timer.start(100, 100, [&](io::Timer& timer) {
        second_callback_called = true;
        timer.stop();
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_FALSE(first_callback_called);
    EXPECT_TRUE(second_callback_called);
}

TEST_F(TimerTest, multiple_intervals) {
    io::EventLoop loop;

    const std::deque<std::uint64_t> intervals = {100, 200, 300};

    using ElapsedDuration = std::chrono::duration<float, std::milli>;
    std::deque<ElapsedDuration> durations;

    const auto start_time = std::chrono::system_clock::now();

    io::Timer timer(loop);
    timer.start(intervals,
        [&](io::Timer& timer) {
            durations.push_back(std::chrono::system_clock::now() - start_time);
        }
    );

    ASSERT_EQ(0, durations.size());

    ASSERT_EQ(0, loop.run());

    ASSERT_EQ(3, durations.size());
    for (std::size_t i = 0 ; i < durations.size(); ++i) {
        EXPECT_NEAR(durations[i].count(), intervals[i], 10);
    }
}

// TOOD: double start with different values

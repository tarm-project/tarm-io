#include "UTCommon.h"

#include "io/Timer.h"

#include <chrono>

struct TimerTest : public testing::Test,
                   public LogRedirector {

};

// Note: in some tests there are double loop.run() calls. This is made
//       to ensure that timer stopping will exit the loop.

TEST_F(TimerTest, constructor) {
    io::EventLoop loop;

    auto timer = new io::Timer(loop);

    ASSERT_EQ(0, loop.run());

    timer->schedule_removal();
    ASSERT_EQ(0, loop.run());
}

TEST_F(TimerTest, schedule_with_no_repeat) {
    io::EventLoop loop;

    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time = start_time;

    const uint64_t TIMEOUT_MS = 100;
    size_t call_counter = 0;

    auto timer = new io::Timer(loop);
    timer->start(TIMEOUT_MS, 0, [&](io::Timer& timer) {
        end_time = std::chrono::high_resolution_clock::now();
        ++call_counter;
    });

    ASSERT_EQ(0, loop.run());

    const auto timer_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    EXPECT_GE(timer_duration, TIMEOUT_MS - 10); // -10 because sometime timer may wake up a bit earlier

    EXPECT_EQ(1, call_counter);

    timer->schedule_removal();
    ASSERT_EQ(0, loop.run());
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

    auto timer = new io::Timer(loop);
    timer->start(0, 0, [&](io::Timer& timer) {
        ASSERT_EQ(0, idle_counter);

        end_time = std::chrono::high_resolution_clock::now();
        ++timer_counter;
    });

    ASSERT_EQ(0, loop.run());

    const auto timer_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    EXPECT_LE(timer_duration, 100);

    EXPECT_EQ(1, timer_counter);

    timer->schedule_removal();
    ASSERT_EQ(0, loop.run());
}

TEST_F(TimerTest, no_callback) {
    // Note: this case handles some sort of idle to prevent loop from exit during desired time
    io::EventLoop loop;

    auto timer = new io::Timer(loop);
    timer->start(100, nullptr);

    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time = start_time;

    ASSERT_EQ(0, loop.run());
    const auto timer_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    EXPECT_LE(timer_duration, 100);

    timer->schedule_removal();
    ASSERT_EQ(0, loop.run());
}

TEST_F(TimerTest, stop_after_start) {
    io::EventLoop loop;

    bool callback_called = false;

    auto timer = new io::Timer(loop);
    timer->start(100, [&](io::Timer& ) {
        callback_called = true;
    });
    timer->stop();

    ASSERT_EQ(0, loop.run());
    EXPECT_FALSE(callback_called);

    timer->schedule_removal();
    ASSERT_EQ(0, loop.run());
}

TEST_F(TimerTest, stop_in_callback) {
    io::EventLoop loop;

    std::size_t calls_counter = 0;

    auto timer = new io::Timer(loop);
    timer->start(100, 100, [&](io::Timer& timer) {
        ++calls_counter;
        timer.stop();
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(1, calls_counter);

    timer->schedule_removal();
    ASSERT_EQ(0, loop.run());
}

TEST_F(TimerTest, start_stop_start_stop) {
    io::EventLoop loop;

    bool first_callback_called = false;
    bool second_callback_called = false;

    auto timer = new io::Timer(loop);
    timer->start(100, 100, [&](io::Timer& timer) {
        first_callback_called = true;
    });
    timer->stop();

    timer->start(100, 100, [&](io::Timer& timer) {
        second_callback_called = true;
        timer.stop();
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_FALSE(first_callback_called);
    EXPECT_TRUE(second_callback_called);

    timer->schedule_removal();
    ASSERT_EQ(0, loop.run());
}

TEST_F(TimerTest, schedule_with_repeat) {
    io::EventLoop loop;

    const uint64_t TIMEOUT_MS = 100;
    const uint64_t REPEAT_MS = 300;
    size_t call_counter = 0;

    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time_1 = start_time;
    auto end_time_2 = start_time;

    auto timer = new io::Timer(loop);
    timer->start(TIMEOUT_MS, REPEAT_MS, [&](io::Timer& timer) {
        if (call_counter == 0) {
            end_time_1 = std::chrono::high_resolution_clock::now();
        } else {
            end_time_2 = std::chrono::high_resolution_clock::now();
            timer.stop();
        }

        ++call_counter;
    });

    EXPECT_EQ(0, call_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(2, call_counter);

    const auto timer_duration_1 = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_1 - start_time).count();
    EXPECT_GE(timer_duration_1, TIMEOUT_MS - 10); // -10 because sometime timer may wake up earlier

    const auto timer_duration_2 = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_2 - end_time_1).count();
    EXPECT_GE(timer_duration_2, REPEAT_MS - 10);

    timer->schedule_removal();
    ASSERT_EQ(0, loop.run());
}

TEST_F(TimerTest, schedule_removal_after_start) {
    io::EventLoop loop;

    const uint64_t TIMEOUT_MS = 100;
    const uint64_t REPEAT_MS = 300;

    size_t call_counter = 0;

    auto timer = new io::Timer(loop);
    timer->start(TIMEOUT_MS, REPEAT_MS, [&](io::Timer& timer) {
        ++call_counter;
    });

    timer->schedule_removal();

    EXPECT_EQ(0, call_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(0, call_counter);
}

TEST_F(TimerTest, schedule_removal_from_callback) {
    io::EventLoop loop;

    const uint64_t TIMEOUT_MS = 100;
    const uint64_t REPEAT_MS = 300;

    size_t call_counter = 0;

    auto timer = new io::Timer(loop);
    timer->start(TIMEOUT_MS, REPEAT_MS,
        [&](io::Timer& timer) {
            ++call_counter;
            timer.schedule_removal();
        }
    );

    EXPECT_EQ(0, call_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, call_counter);
}

TEST_F(TimerTest, multiple_intervals) {
    io::EventLoop loop;

    const std::deque<std::uint64_t> intervals = {100, 200, 300};

    using ElapsedDuration = std::chrono::duration<float, std::milli>;
    std::deque<ElapsedDuration> durations;

    const auto start_time = std::chrono::system_clock::now();

    auto timer = new io::Timer(loop);
    timer->start(intervals,
        [&](io::Timer& timer) {
            durations.push_back(std::chrono::system_clock::now() - start_time);
        }
    );

    ASSERT_EQ(0, durations.size());

    ASSERT_EQ(0, loop.run());

    ASSERT_EQ(3, durations.size());
    for (std::size_t i = 0 ; i < durations.size(); ++i) {
        EXPECT_NEAR(durations[i].count(), intervals[i], 25); // Making test valgrind-friendly // TODO: need to revise this
    }

    timer->schedule_removal();
    ASSERT_EQ(0, loop.run());
}

TEST_F(TimerTest, 100k_timers) {
    io::EventLoop loop;

    std::size_t callback_counter = 0;
    auto common_callback = [&](io::Timer& timer) {
        ++callback_counter;
        timer.schedule_removal();
    };

    const std::size_t COUNT = 100000;
    for (std::size_t i = 0; i < COUNT; ++i) {
        auto timer = new io::Timer(loop);
        timer->start(i % 500 + 1, common_callback);
    }

    EXPECT_EQ(0, callback_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(COUNT, callback_counter);
}

// TOOD: multiple start with different values

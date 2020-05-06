/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "io/Timer.h"

#include <chrono>
#include <unordered_map>

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
    EXPECT_EQ(0, timer->timeout_ms());
    EXPECT_EQ(0, timer->repeat_ms());
    timer->start(TIMEOUT_MS, REPEAT_MS, [&](io::Timer& timer) {
        EXPECT_EQ(TIMEOUT_MS, timer.timeout_ms());
        EXPECT_EQ(REPEAT_MS, timer.repeat_ms());

        if (call_counter == 0) {
            end_time_1 = std::chrono::high_resolution_clock::now();
        } else {
            end_time_2 = std::chrono::high_resolution_clock::now();
            timer.stop();
        }

        ++call_counter;
    });
    EXPECT_EQ(TIMEOUT_MS, timer->timeout_ms());
    EXPECT_EQ(REPEAT_MS, timer->repeat_ms());

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

    auto start_time = std::chrono::high_resolution_clock::now();

    auto timer = new io::Timer(loop);
    timer->start(intervals,
        [&](io::Timer& timer) {
            EXPECT_EQ(intervals[durations.size()], timer.timeout_ms());
            EXPECT_EQ(0, timer.repeat_ms());
            const auto now = std::chrono::high_resolution_clock::now();
            durations.push_back(now - start_time);
            start_time = now;
        }
    );
    EXPECT_EQ(intervals[0], timer->timeout_ms());
    EXPECT_EQ(0, timer->repeat_ms());

    ASSERT_EQ(0, durations.size());

    ASSERT_EQ(0, loop.run());

    ASSERT_EQ(3, durations.size());
    for (std::size_t i = 0 ; i < durations.size(); ++i) {
        EXPECT_NEAR(durations[i].count(), intervals[i], 30); // Making test valgrind-friendly // TODO: need to revise this
    }

    timer->schedule_removal();
    ASSERT_EQ(0, loop.run());
}

TEST_F(TimerTest, start_in_callback_1) {
    io::EventLoop loop;

    std::size_t call_counter = 0;

    auto timer = new io::Timer(loop);
    timer->start(10, [&](io::Timer& timer) {
        ++call_counter;
        EXPECT_EQ(10, timer.timeout_ms());
        timer.start(20, [&](io::Timer& timer) {
            ++call_counter;
            EXPECT_EQ(20, timer.timeout_ms());
            timer.start(30, [&](io::Timer& timer) {
                ++call_counter;
                EXPECT_EQ(30, timer.timeout_ms());
                timer.schedule_removal();
            });
            EXPECT_EQ(30, timer.timeout_ms());
        });
        EXPECT_EQ(20, timer.timeout_ms());
    });
    EXPECT_EQ(10, timer->timeout_ms());

    EXPECT_EQ(0, call_counter);

    EXPECT_EQ(0, loop.run());

    EXPECT_EQ(3, call_counter);
}

TEST_F(TimerTest, start_in_callback_2) {
    io::EventLoop loop;

    std::size_t call_counter = 0;

    auto timer = new io::Timer(loop);
    timer->start(10, 100, [&](io::Timer& timer) {
        ++call_counter;
        EXPECT_EQ(10, timer.timeout_ms());
        timer.start(20, 200, [&](io::Timer& timer) {
            ++call_counter;
            EXPECT_EQ(20, timer.timeout_ms());
            timer.start(30, 300, [&](io::Timer& timer) {
                ++call_counter;
                EXPECT_EQ(30, timer.timeout_ms());
                timer.schedule_removal();
            });
            EXPECT_EQ(30, timer.timeout_ms());
        });
        EXPECT_EQ(20, timer.timeout_ms());
    });
    EXPECT_EQ(10, timer->timeout_ms());

    EXPECT_EQ(0, call_counter);

    EXPECT_EQ(0, loop.run());

    EXPECT_EQ(3, call_counter);
}

TEST_F(TimerTest, start_in_callback_3) {
    io::EventLoop loop;

    std::size_t call_counter = 0;

    auto timer = new io::Timer(loop);
    timer->start({1, 2, 3}, [&](io::Timer& timer) {
        ++call_counter;
        EXPECT_EQ(1, timer.timeout_ms());
        timer.start({10, 20, 30}, [&](io::Timer& timer) {
            ++call_counter;
            EXPECT_EQ(10, timer.timeout_ms());
            timer.start({100, 200, 300}, [&](io::Timer& timer) {
                ++call_counter;
                EXPECT_EQ(100, timer.timeout_ms());
                timer.schedule_removal();
            });
            EXPECT_EQ(100, timer.timeout_ms());
        });
        EXPECT_EQ(10, timer.timeout_ms());
    });
    EXPECT_EQ(1, timer->timeout_ms());

    EXPECT_EQ(0, call_counter);

    EXPECT_EQ(0, loop.run());

    EXPECT_EQ(3, call_counter);
}

TEST_F(TimerTest, callback_call_counter_1) {
    io::EventLoop loop;

    const std::size_t CALL_COUNT_MAX = 10;

    std::size_t callback_counter = 0;

    std::function<void(io::Timer&)> timer_callback = [&] (io::Timer& timer) {
        EXPECT_EQ(0, timer.callback_call_counter());
        ++callback_counter;
        if (callback_counter == CALL_COUNT_MAX) {
            timer.schedule_removal();
        } else {
            timer.start(10, timer_callback);
        }

        // TODO: if increment counter here on Ubuntu 14.04.6 (GCC 4.8.4), memory corruption happen
        //       need some workaround for such kind of problems and not only for Timer
        // ++callback_counter;
    };

    auto timer = new io::Timer(loop);
    EXPECT_EQ(0, timer->callback_call_counter());
    timer->start(10, timer_callback);
    EXPECT_EQ(0, timer->callback_call_counter());

    EXPECT_EQ(0, callback_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(CALL_COUNT_MAX, callback_counter);
}

TEST_F(TimerTest, callback_call_counter_2) {
    io::EventLoop loop;

    const std::size_t CALL_COUNT_MAX = 10;

    std::size_t callback_counter = 0;

    auto timer = new io::Timer(loop);
    EXPECT_EQ(0, timer->callback_call_counter());
    timer->start(10, 20, [&] (io::Timer& timer) {
        EXPECT_EQ(callback_counter, timer.callback_call_counter());
        if (callback_counter == CALL_COUNT_MAX - 1) {
            timer.schedule_removal();
        }
        ++callback_counter;
    });
    EXPECT_EQ(0, timer->callback_call_counter());

    EXPECT_EQ(0, callback_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(CALL_COUNT_MAX, callback_counter);
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

TEST_F(TimerTest, 1k_timers_1k_timeouts) {
    io::EventLoop loop;

    std::size_t callback_counter = 0;
    std::unordered_map<std::size_t, std::size_t> callbacks_stats;

    const std::size_t COUNT = 1000;

    auto common_callback = [&](io::Timer& timer) {
        std::size_t timer_id = reinterpret_cast<std::size_t>(timer.user_data());
        callbacks_stats[timer_id]++;
        ++callback_counter;
        if (callbacks_stats[timer_id] == COUNT) {
            timer.schedule_removal();
        }
    };

    for (std::size_t i = 0; i < COUNT; ++i) {
        auto timer = new io::Timer(loop);
        timer->set_user_data(reinterpret_cast<void*>(i));
        timer->start(1, 1, common_callback);
    }

    EXPECT_EQ(0, callback_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(COUNT * COUNT, callback_counter);

    for(auto& k_v : callbacks_stats) {
        ASSERT_EQ(COUNT, k_v.second) << k_v.first;
    }
}

TEST_F(TimerTest, multiple_starts) {
    io::EventLoop loop;

    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time = start_time;

    const uint64_t TIMEOUT_1_MS = 100;
    size_t call_counter_1 = 0;

    const uint64_t TIMEOUT_2_MS = 200;
    size_t call_counter_2 = 0;

    auto timer = new io::Timer(loop);
    timer->start(TIMEOUT_1_MS, 0, [&](io::Timer& timer) {
        ++call_counter_1;
        timer.schedule_removal();
    });

    timer->start(TIMEOUT_2_MS, 0, [&](io::Timer& timer) {
        end_time = std::chrono::high_resolution_clock::now();
        ++call_counter_2;
        timer.schedule_removal();
    });

    EXPECT_EQ(0, call_counter_1);
    EXPECT_EQ(0, call_counter_2);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(0, call_counter_1);
    EXPECT_EQ(1, call_counter_2);

    const auto timer_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    EXPECT_GE(timer_duration, TIMEOUT_2_MS - 10); // -10 because sometime timer may wake up a bit earlier
}

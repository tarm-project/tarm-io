/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "Timer.h"

#include <unordered_map>

struct TimerTest : public testing::Test,
                   public LogRedirector {

};

// Note: in some tests there are double loop.run() calls. This is made
//       to ensure that timer stopping will exit the loop.

TEST_F(TimerTest, allocate) {
    io::EventLoop loop;

    //auto timer = new io::Timer(loop);
    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    timer->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TimerTest, schedule_with_no_repeat) {
    io::EventLoop loop;

    const uint64_t TIMEOUT_MS = 300;
    size_t call_counter = 0;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
    timer->start(TIMEOUT_MS, 0, [&](io::Timer& timer) {
        ++call_counter;
        EXPECT_TIMEOUT_MS(TIMEOUT_MS, timer.real_time_passed_since_last_callback());
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, call_counter);

    timer->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TimerTest, zero_timeot) {
    // Note: in this test we check that timer is called on next loop cycle

    io::EventLoop loop;

    std::size_t timer_counter = 0;
    std::size_t idle_counter = 0;

    std::size_t idle_handle = io::EventLoop::INVALID_HANDLE;
    idle_handle = loop.schedule_call_on_each_loop_cycle([&](io::EventLoop& loop) {
        ASSERT_EQ(1, timer_counter);
        ++idle_counter;

        loop.stop_call_on_each_loop_cycle(idle_handle);
    });

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
    timer->start(0, 0, [&](io::Timer& timer) {
        ASSERT_EQ(0, idle_counter);
        ++timer_counter;
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, timer_counter);

    timer->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TimerTest, no_callback) {
    // Note: this case handles some sort of idle to prevent loop from exit during desired time.
    //       Even if timer has no callback, it should block an EventLoop.
    io::EventLoop loop;

    const std::size_t TIMEOUT_MS = 300;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
    timer->start(TIMEOUT_MS, nullptr);

    auto t1 = std::chrono::high_resolution_clock::now();

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    auto t2 = std::chrono::high_resolution_clock::now();

    EXPECT_LE(timer->real_time_passed_since_last_callback().count(), 10);
    EXPECT_TIMEOUT_MS(TIMEOUT_MS, std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1));

    timer->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TimerTest, stop_after_start) {
    io::EventLoop loop;

    bool callback_called = false;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
    timer->start(100, [&](io::Timer& ) {
        callback_called = true;
    });
    timer->stop();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_FALSE(callback_called);

    timer->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TimerTest, stop_in_callback) {
    io::EventLoop loop;

    std::size_t calls_counter = 0;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
    timer->start(100, 100, [&](io::Timer& timer) {
        ++calls_counter;
        timer.stop();
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_EQ(1, calls_counter);

    timer->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TimerTest, start_stop_start_stop) {
    io::EventLoop loop;

    bool first_callback_called = false;
    bool second_callback_called = false;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
    timer->start(100, 100, [&](io::Timer& timer) {
        first_callback_called = true;
    });
    timer->stop();

    timer->start(100, 100, [&](io::Timer& timer) {
        second_callback_called = true;
        timer.stop();
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_FALSE(first_callback_called);
    EXPECT_TRUE(second_callback_called);

    timer->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TimerTest, schedule_with_repeat) {
    io::EventLoop loop;

    const uint64_t TIMEOUT_MS = 600;
    const uint64_t REPEAT_MS = 300;
    size_t call_counter = 0;

    std::chrono::milliseconds duration_1;
    std::chrono::milliseconds duration_2;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
    EXPECT_EQ(0, timer->timeout_ms());
    EXPECT_EQ(0, timer->repeat_ms());
    timer->start(TIMEOUT_MS, REPEAT_MS, [&](io::Timer& timer) {
        EXPECT_EQ(TIMEOUT_MS, timer.timeout_ms());
        EXPECT_EQ(REPEAT_MS, timer.repeat_ms());

        if (call_counter == 0) {
            duration_1 = timer.real_time_passed_since_last_callback();
        } else {
            duration_2 = timer.real_time_passed_since_last_callback();
            timer.stop();
        }

        ++call_counter;
    });
    EXPECT_EQ(TIMEOUT_MS, timer->timeout_ms());
    EXPECT_EQ(REPEAT_MS, timer->repeat_ms());

    EXPECT_EQ(0, call_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(2, call_counter);

    EXPECT_GT(duration_1, duration_2);
    EXPECT_TIMEOUT_MS(TIMEOUT_MS, duration_1);
    EXPECT_TIMEOUT_MS(REPEAT_MS, duration_2);

    timer->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TimerTest, schedule_removal_after_start) {
    io::EventLoop loop;

    const uint64_t TIMEOUT_MS = 100;
    const uint64_t REPEAT_MS = 300;

    size_t call_counter = 0;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
    timer->start(TIMEOUT_MS, REPEAT_MS, [&](io::Timer& timer) {
        ++call_counter;
    });

    timer->schedule_removal();

    EXPECT_EQ(0, call_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(0, call_counter);
}

TEST_F(TimerTest, schedule_removal_from_callback) {
    io::EventLoop loop;

    const uint64_t TIMEOUT_MS = 100;
    const uint64_t REPEAT_MS = 300;

    size_t call_counter = 0;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
    timer->start(TIMEOUT_MS, REPEAT_MS,
        [&](io::Timer& timer) {
            ++call_counter;
            timer.schedule_removal();
        }
    );

    EXPECT_EQ(0, call_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, call_counter);
}

TEST_F(TimerTest, multiple_intervals) {
    io::EventLoop loop;

    const std::deque<std::uint64_t> intervals = {300, 500, 700};

    std::deque<std::chrono::milliseconds> durations;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
    timer->start(intervals,
        [&](io::Timer& timer) {
            EXPECT_EQ(intervals[durations.size()], timer.timeout_ms());
            EXPECT_EQ(0, timer.repeat_ms());
            durations.push_back(timer.real_time_passed_since_last_callback());
        }
    );
    EXPECT_EQ(intervals[0], timer->timeout_ms());
    EXPECT_EQ(0, timer->repeat_ms());

    ASSERT_EQ(0, durations.size());

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    ASSERT_EQ(3, durations.size());
    for (std::size_t i = 0 ; i < durations.size(); ++i) {
        EXPECT_TIMEOUT_MS(intervals[i], durations[i].count()) << "i= " << i;
    }

    timer->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(TimerTest, start_in_callback_1) {
    io::EventLoop loop;

    std::size_t call_counter = 0;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
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

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(3, call_counter);
}

TEST_F(TimerTest, start_in_callback_2) {
    io::EventLoop loop;

    std::size_t call_counter = 0;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
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

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(3, call_counter);
}

TEST_F(TimerTest, start_in_callback_3) {
    io::EventLoop loop;

    std::size_t call_counter = 0;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
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

    EXPECT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(3, call_counter);
}

TEST_F(TimerTest, callback_call_counter_1) {
    io::EventLoop loop;

    const std::size_t CALL_COUNT_MAX = 10;

    std::size_t callback_counter = 0;

    std::function<void(io::Timer&)> timer_callback = [&] (io::Timer& timer) {
        EXPECT_EQ(0, timer.callback_call_counter());
        if (callback_counter == CALL_COUNT_MAX - 1) {
            timer.schedule_removal();
        } else {
            timer.start(10, timer_callback);
        }

        ++callback_counter;
    };

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
    EXPECT_EQ(0, timer->callback_call_counter());
    timer->start(10, timer_callback);
    EXPECT_EQ(0, timer->callback_call_counter());

    EXPECT_EQ(0, callback_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(CALL_COUNT_MAX, callback_counter);
}

TEST_F(TimerTest, callback_call_counter_2) {
    io::EventLoop loop;

    const std::size_t CALL_COUNT_MAX = 10;

    std::size_t callback_counter = 0;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
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

    ASSERT_EQ(io::StatusCode::OK, loop.run());

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
        auto timer = loop.allocate<io::Timer>();
        ASSERT_TRUE(timer) << "i=" << i;
        timer->start(i % 500 + 1, common_callback);
    }

    EXPECT_EQ(0, callback_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

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
        auto timer = loop.allocate<io::Timer>();
        ASSERT_TRUE(timer) << loop.last_allocation_error() << "i=" << i;
        timer->set_user_data(reinterpret_cast<void*>(i));
        timer->start(1, 1, common_callback);
    }

    EXPECT_EQ(0, callback_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(COUNT * COUNT, callback_counter);

    for(auto& k_v : callbacks_stats) {
        ASSERT_EQ(COUNT, k_v.second) << k_v.first;
    }
}

TEST_F(TimerTest, multiple_starts) {
    io::EventLoop loop;

    const uint64_t TIMEOUT_1_MS = 300;
    size_t call_counter_1 = 0;

    const uint64_t TIMEOUT_2_MS = 500;
    size_t call_counter_2 = 0;

    auto timer = loop.allocate<io::Timer>();
    ASSERT_TRUE(timer) << loop.last_allocation_error();
    timer->start(TIMEOUT_1_MS, 0, [&](io::Timer& timer) {
        // This will not be executed
        ++call_counter_1;
        timer.schedule_removal();
    });

    timer->start(TIMEOUT_2_MS, 0, [&](io::Timer& timer) {
        EXPECT_TIMEOUT_MS(TIMEOUT_2_MS, timer.real_time_passed_since_last_callback());
        ++call_counter_2;
        timer.schedule_removal();
    });

    EXPECT_EQ(0, call_counter_1);
    EXPECT_EQ(0, call_counter_2);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(0, call_counter_1);
    EXPECT_EQ(1, call_counter_2);
}

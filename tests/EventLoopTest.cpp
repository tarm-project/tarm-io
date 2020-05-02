/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "io/EventLoop.h"
#include "io/ScopeExitGuard.h"

#include <thread>
#include <mutex>

struct EventLoopTest : public testing::Test,
                       public LogRedirector {
};

TEST_F(EventLoopTest, default_constructor) {
    std::unique_ptr<io::EventLoop> event_loop;
    EXPECT_NO_THROW(event_loop.reset(new io::EventLoop));
    ASSERT_EQ(0, event_loop->run());
}

TEST_F(EventLoopTest, default_constructor_with_run) {
    io::EventLoop event_loop;
    ASSERT_EQ(0, event_loop.run());
}

TEST_F(EventLoopTest, work_no_work_done_callback) {
    bool callback_executed = false;

    io::EventLoop event_loop;
    event_loop.add_work([&]() {
        callback_executed = true;
    });

    ASSERT_EQ(0, event_loop.run());
    ASSERT_TRUE(callback_executed);
}

TEST_F(EventLoopTest, work_all_callbacks) {
    bool callback_executed = false;
    bool done_executed = false;

    io::EventLoop event_loop;
    event_loop.add_work([&]() {
        callback_executed = true;
        ASSERT_FALSE(done_executed);
    },
    [&]() {
        done_executed = true;
    });

    ASSERT_EQ(0, event_loop.run());
    ASSERT_TRUE(callback_executed);
    ASSERT_TRUE(done_executed);
}

TEST_F(EventLoopTest, only_work_done_callback) {
    // invalid case
    bool done_executed = false;

    io::EventLoop event_loop;
    event_loop.add_work(
        nullptr,
        [&]() {
            done_executed = true;
        }
    );

    ASSERT_EQ(0, event_loop.run());
    ASSERT_FALSE(done_executed);
}

TEST_F(EventLoopTest, work_with_user_data) {
    bool callback_executed = false;
    bool done_executed = false;

    io::EventLoop event_loop;
    event_loop.add_work(
        [&]() -> void* {
            callback_executed = true;
            return new int(42);
        },
        [&](void* user_data) {
            done_executed = true;
            auto& value = *reinterpret_cast<int*>(user_data);
            EXPECT_EQ(42, value);
            delete &value;
        }
    );

    ASSERT_EQ(0, event_loop.run());
    ASSERT_TRUE(callback_executed);
    ASSERT_TRUE(done_executed);
}

TEST_F(EventLoopTest, schedule_on_each_loop_cycle) {
    io::EventLoop loop;

    size_t counter = 0;
    size_t handle = io::EventLoop::INVALID_HANDLE;

    handle = loop.schedule_call_on_each_loop_cycle([&handle, &counter, &loop]() {
        ++counter;

        if (counter == 500) {
            loop.stop_call_on_each_loop_cycle(handle);
        }
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(500, counter);
}

TEST_F(EventLoopTest, stop_call_on_each_loop_cycle_with_invalid_data) {
    // Crash test
    io::EventLoop loop;
    loop.stop_call_on_each_loop_cycle(100500);
    loop.stop_call_on_each_loop_cycle(io::EventLoop::INVALID_HANDLE);
    ASSERT_EQ(0, loop.run());
}

TEST_F(EventLoopTest, multiple_schedule_on_each_loop_cycle) {
    io::EventLoop loop;

    size_t counter_1 = 0;
    size_t handle_1 = io::EventLoop::INVALID_HANDLE;
    size_t counter_2 = 0;
    size_t handle_2 = io::EventLoop::INVALID_HANDLE;
    size_t counter_3 = 0;
    size_t handle_3 = io::EventLoop::INVALID_HANDLE;

    handle_1 = loop.schedule_call_on_each_loop_cycle([&handle_1, &counter_1, &loop]() {
        ++counter_1;

        if (counter_1 == 500) {
            loop.stop_call_on_each_loop_cycle(handle_1);
        }
    });

    handle_2 = loop.schedule_call_on_each_loop_cycle([&handle_2, &counter_2, &loop]() {
        ++counter_2;

        if (counter_2 == 200) {
            loop.stop_call_on_each_loop_cycle(handle_2);
        }
    });

    handle_3 = loop.schedule_call_on_each_loop_cycle([&handle_3, &counter_3, &loop]() {
        ++counter_3;

        if (counter_3 == 300) {
            loop.stop_call_on_each_loop_cycle(handle_3);
        }
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(500, counter_1);
    EXPECT_EQ(200, counter_2);
    EXPECT_EQ(300, counter_3);
}

TEST_F(EventLoopTest, is_running) {
    io::EventLoop event_loop;

    std::size_t callback_call_count = 0;

    std::size_t handle = io::EventLoop::INVALID_HANDLE;
    handle = event_loop.schedule_call_on_each_loop_cycle([&]() {
        EXPECT_TRUE(event_loop.is_running());
        event_loop.stop_call_on_each_loop_cycle(handle);
        ++callback_call_count;
    });

    EXPECT_EQ(0, callback_call_count);
    EXPECT_FALSE(event_loop.is_running());
    ASSERT_EQ(0, event_loop.run());
    EXPECT_FALSE(event_loop.is_running());
    EXPECT_EQ(1, callback_call_count);
}

TEST_F(EventLoopTest, loop_in_thread) {
    std::mutex data_mutex;
    size_t combined_counter = 0;

    auto functor = [&combined_counter, &data_mutex]() {
        io::EventLoop loop;
        size_t counter = 0;

        const size_t handle = loop.schedule_call_on_each_loop_cycle([&counter, &loop, &handle]() {
            ++counter;

            if (counter == 200) {
                loop.stop_call_on_each_loop_cycle(handle);
            }
        });

        loop.run();

        EXPECT_EQ(counter, 200);
        std::lock_guard<decltype(data_mutex)> guard(data_mutex);
        combined_counter += counter;
    };

    std::thread thread_1(functor);
    std::thread thread_2(functor);
    std::thread thread_3(functor);

    thread_1.join();
    thread_2.join();
    thread_3.join();

    EXPECT_EQ(combined_counter, 600);
}

TEST_F(EventLoopTest, dummy_idle) {
    io::EventLoop loop;

    // crash tests
    loop.start_dummy_idle();
    loop.start_dummy_idle();
    loop.stop_dummy_idle();
    loop.stop_dummy_idle();
    loop.start_dummy_idle();
    loop.stop_dummy_idle();

    ASSERT_EQ(0, loop.run());
}

TEST_F(EventLoopTest, execute_on_loop_thread_from_main_thread) {
    io::EventLoop loop;

    auto main_thread_id = std::this_thread::get_id();
    int execute_on_loop_thread_call_counter = 0;

    loop.execute_on_loop_thread([&main_thread_id, &execute_on_loop_thread_call_counter](){
        ASSERT_EQ(main_thread_id, std::this_thread::get_id());
        ++execute_on_loop_thread_call_counter;
    });

    EXPECT_EQ(0, execute_on_loop_thread_call_counter);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(0, execute_on_loop_thread_call_counter);
    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(1, execute_on_loop_thread_call_counter);
}

TEST_F(EventLoopTest, execute_on_loop_thread_nested) {
    io::EventLoop loop;

    auto main_thread_id = std::this_thread::get_id();
    int execute_on_loop_thread_call_counter_1 = 0;
    int execute_on_loop_thread_call_counter_2 = 0;

    loop.execute_on_loop_thread([&]() {
        ASSERT_EQ(main_thread_id, std::this_thread::get_id());
        ++execute_on_loop_thread_call_counter_1;

        loop.execute_on_loop_thread([&]() {
            ASSERT_EQ(main_thread_id, std::this_thread::get_id());
            ++execute_on_loop_thread_call_counter_2;
        });
    });

    EXPECT_EQ(0, execute_on_loop_thread_call_counter_1);
    EXPECT_EQ(0, execute_on_loop_thread_call_counter_2);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(0, execute_on_loop_thread_call_counter_1);
    EXPECT_EQ(0, execute_on_loop_thread_call_counter_2);
    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(1, execute_on_loop_thread_call_counter_1);
    EXPECT_EQ(1, execute_on_loop_thread_call_counter_2);
}

TEST_F(EventLoopTest, execute_on_loop_thread_from_other_thread) {
    io::EventLoop loop;

    auto main_thread_id = std::this_thread::get_id();
    bool execute_on_loop_thread_called = false;

    loop.start_dummy_idle(); // need to hold loop running

    std::thread thread([&loop, &execute_on_loop_thread_called, &main_thread_id](){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        loop.execute_on_loop_thread([&loop, &execute_on_loop_thread_called, &main_thread_id](){
            ASSERT_EQ(main_thread_id, std::this_thread::get_id());
            execute_on_loop_thread_called = true;
            loop.stop_dummy_idle();
        });
    });

    io::ScopeExitGuard scope_guard([&thread](){
        thread.join();
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(execute_on_loop_thread_called);
}

TEST_F(EventLoopTest, run_loop_several_times) {
    io::EventLoop loop;

    int counter_1 = 0;
    int counter_2 = 0;
    int counter_3 = 0;

    loop.execute_on_loop_thread([&]() {
        ++counter_1;
    });

    EXPECT_EQ(0, counter_1);
    EXPECT_EQ(0, counter_2);
    EXPECT_EQ(0, counter_3);

    ASSERT_EQ(0, loop.run()); // run 1

    EXPECT_EQ(1, counter_1);
    EXPECT_EQ(0, counter_2);
    EXPECT_EQ(0, counter_3);

    loop.execute_on_loop_thread([&]() {
        ++counter_2;
    });

    EXPECT_EQ(1, counter_1);
    EXPECT_EQ(0, counter_2);
    EXPECT_EQ(0, counter_3);

    ASSERT_EQ(0, loop.run()); // run 2

    EXPECT_EQ(1, counter_1);
    EXPECT_EQ(1, counter_2);
    EXPECT_EQ(0, counter_3);

    loop.execute_on_loop_thread([&]() {
        ++counter_3;
    });

    EXPECT_EQ(1, counter_1);
    EXPECT_EQ(1, counter_2);
    EXPECT_EQ(0, counter_3);

    ASSERT_EQ(0, loop.run()); // run 3

    EXPECT_EQ(1, counter_1);
    EXPECT_EQ(1, counter_2);
    EXPECT_EQ(1, counter_3);
}

// TODO: create event loop in one thread and run in another

TEST_F(EventLoopTest, schedule_1_callback) {
    io::EventLoop loop;

    std::size_t callback_counter = 0;
    loop.schedule_callback([&](){
        callback_counter++;
    });

    EXPECT_EQ(0, callback_counter);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, callback_counter);
}

TEST_F(EventLoopTest, schedule_callback_after_loop_run) {
    std::size_t callback_counter = 0;
    io::ScopeExitGuard scope_guard([&](){
        EXPECT_EQ(0, callback_counter);
    });

    io::EventLoop loop;

    EXPECT_EQ(0, callback_counter);

    ASSERT_EQ(0, loop.run());

    loop.schedule_callback([&](){
        callback_counter++;
    });

    EXPECT_EQ(0, callback_counter);
}

TEST_F(EventLoopTest, schedule_multiple_callbacks) {
    io::EventLoop loop;

    std::size_t callback_counter_1 = 0;
    std::size_t callback_counter_2 = 0;
    std::size_t callback_counter_3 = 0;

    auto callback_1  = [&]() {
        ++callback_counter_1;
    };

    auto callback_2  = [&]() {
        ++callback_counter_2;
    };

    auto callback_3  = [&]() {
        ++callback_counter_3;
    };

    loop.schedule_callback(callback_1);
    loop.schedule_callback(callback_2);
    loop.schedule_callback(callback_3);

    EXPECT_EQ(0, callback_counter_1);
    EXPECT_EQ(0, callback_counter_2);
    EXPECT_EQ(0, callback_counter_3);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, callback_counter_1);
    EXPECT_EQ(1, callback_counter_2);
    EXPECT_EQ(1, callback_counter_3);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, callback_counter_1);
    EXPECT_EQ(1, callback_counter_2);
    EXPECT_EQ(1, callback_counter_3);

    loop.schedule_callback(callback_1);
    loop.schedule_callback(callback_2);

    EXPECT_EQ(1, callback_counter_1);
    EXPECT_EQ(1, callback_counter_2);
    EXPECT_EQ(1, callback_counter_3);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(2, callback_counter_1);
    EXPECT_EQ(2, callback_counter_2);
    EXPECT_EQ(1, callback_counter_3);
}

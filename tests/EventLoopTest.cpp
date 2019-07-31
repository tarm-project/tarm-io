#include "UTCommon.h"

#include "io/EventLoop.h"
#include "io/ScopeExitGuard.h"

#include <thread>
#include <mutex>

struct EventLoopTest : public testing::Test,
                       public LogRedirector {

};

TEST_F(EventLoopTest, default_constructor) {
    io::EventLoop event_loop;
    ASSERT_EQ(0, event_loop.run());
}

TEST_F(EventLoopTest, work_null) {
    io::EventLoop event_loop;
    event_loop.add_work(nullptr);

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

TEST_F(EventLoopTest, schedule_on_each_loop_cycle) {
    io::EventLoop loop;

    size_t counter = 0;
    size_t handle = 0;

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
    io::EventLoop loop;
    loop.stop_call_on_each_loop_cycle(100500);
    ASSERT_EQ(0, loop.run());
}

TEST_F(EventLoopTest, multiple_schedule_on_each_loop_cycle) {
    io::EventLoop loop;

    size_t counter_1 = 0;
    size_t handle_1 = 0;
    size_t counter_2 = 0;
    size_t handle_2 = 0;
    size_t counter_3 = 0;
    size_t handle_3 = 0;

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

    std::size_t handle = 0; // TODO: make some value as invalid handle
    handle = event_loop.schedule_call_on_each_loop_cycle([&]() {
        EXPECT_TRUE(event_loop.is_running());
        event_loop.stop_call_on_each_loop_cycle(handle);
    });

    EXPECT_FALSE(event_loop.is_running());
    ASSERT_EQ(0, event_loop.run());
    EXPECT_FALSE(event_loop.is_running());
}

TEST_F(EventLoopTest, loop_in_thread) {
    std::mutex data_mutex;
    size_t combined_counter = 0;

    auto functor = [&combined_counter, &data_mutex]() {
        io::EventLoop loop;
        size_t counter = 0;

        size_t handle = loop.schedule_call_on_each_loop_cycle([&counter, &loop, &handle]() {
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
    bool execute_on_loop_thread_called = false;

    loop.execute_on_loop_thread([&main_thread_id, &execute_on_loop_thread_called](){
        ASSERT_EQ(main_thread_id, std::this_thread::get_id());
        execute_on_loop_thread_called = true;
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(execute_on_loop_thread_called);
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

// TODO: same loop run several times with different callbacks each time

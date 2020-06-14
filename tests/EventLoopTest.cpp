/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "io/EventLoop.h"
#include "io/ScopeExitGuard.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    #include <signal.h>
#endif

struct EventLoopTest : public testing::Test,
                       public LogRedirector {
};

TEST_F(EventLoopTest, default_constructor) {
    std::unique_ptr<io::EventLoop> event_loop;
    EXPECT_NO_THROW(event_loop.reset(new io::EventLoop));
    ASSERT_EQ(io::StatusCode::OK, event_loop->run());
}

TEST_F(EventLoopTest, default_constructor_with_run) {
    io::EventLoop event_loop;
    ASSERT_EQ(io::StatusCode::OK, event_loop.run());
}

TEST_F(EventLoopTest, work_no_work_done_callback) {
    bool callback_executed = false;

    io::EventLoop event_loop;
    event_loop.add_work([&](io::EventLoop& loop) {
        EXPECT_EQ(&loop, &event_loop);
        callback_executed = true;
    });

    ASSERT_EQ(io::StatusCode::OK, event_loop.run());
    ASSERT_TRUE(callback_executed);
}

TEST_F(EventLoopTest, work_all_callbacks) {
    bool callback_executed = false;
    bool done_executed = false;

    io::EventLoop event_loop;
    event_loop.add_work(
        [&](io::EventLoop&) {
            callback_executed = true;
            ASSERT_FALSE(done_executed);
        },
        [&](io::EventLoop& loop, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            EXPECT_EQ(&loop, &event_loop);
            done_executed = true;
        }
    );

    ASSERT_EQ(io::StatusCode::OK, event_loop.run());
    ASSERT_TRUE(callback_executed);
    ASSERT_TRUE(done_executed);
}

TEST_F(EventLoopTest, only_work_done_callback) {
    // invalid case
    bool done_executed = false;

    io::EventLoop event_loop;
    auto handle = event_loop.add_work(
        nullptr,
        [&](io::EventLoop&, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            done_executed = true;
        }
    );
    EXPECT_EQ(io::EventLoop::INVALID_HANDLE, handle);

    ASSERT_FALSE(done_executed);

    ASSERT_EQ(io::StatusCode::OK, event_loop.run());

    ASSERT_FALSE(done_executed);
}

TEST_F(EventLoopTest, work_with_user_data) {
    bool callback_executed = false;
    bool done_executed = false;

    io::EventLoop event_loop;
    event_loop.add_work(
        [&](io::EventLoop& loop) -> void* {
            EXPECT_EQ(&loop, &event_loop);
            callback_executed = true;
            return new int(42);
        },
        [&](io::EventLoop& loop, void* user_data, const io::Error& error) {
            EXPECT_FALSE(error) << error.string();
            EXPECT_EQ(&loop, &event_loop);
            done_executed = true;
            auto& value = *reinterpret_cast<int*>(user_data);
            EXPECT_EQ(42, value);
            delete &value;
        }
    );

    ASSERT_EQ(io::StatusCode::OK, event_loop.run());
    ASSERT_TRUE(callback_executed);
    ASSERT_TRUE(done_executed);
}

TEST_F(EventLoopTest, work_is_not_started_before_loop_run) {
    io::EventLoop loop;

    std::atomic<std::size_t> work_callback_count(0);

    loop.add_work([&](io::EventLoop&) {
        ++work_callback_count;
    });

    EXPECT_EQ(0, work_callback_count);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(0, work_callback_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, work_callback_count);
}

TEST_F(EventLoopTest, work_cancel_before_loop_run) {
    const auto thread_pool_size = io::global::thread_pool_size();

    std::size_t on_work_done_counter = 0;

    io::EventLoop event_loop;

    auto dummy_work_callback = [](io::EventLoop&) {
        FAIL() << "This callback should not be invoked!";
    };

    auto work_done_callback = [&](io::EventLoop&, const io::Error& error) {
        EXPECT_TRUE(error);
        EXPECT_EQ(io::StatusCode::OPERATION_CANCELED, error.code());
    };

    std::vector<io::EventLoop::WorkHandle> all_handles;

    for (std::size_t i = 0 ; i < thread_pool_size; ++i) {
        const auto handle = event_loop.add_work(
            dummy_work_callback,
            work_done_callback
        );
        EXPECT_NE(io::EventLoop::INVALID_HANDLE, handle) << "i=" << i;
        all_handles.push_back(handle);
    }

    {
        const auto handle = event_loop.add_work(
            dummy_work_callback,
            work_done_callback
        );
        EXPECT_NE(io::EventLoop::INVALID_HANDLE, handle);
        all_handles.push_back(handle);
    }

    for (std::size_t i = 0 ; i < thread_pool_size; ++i) {
        auto error = event_loop.cancel_work(all_handles[i]);
        EXPECT_FALSE(error);
    }

    auto error = event_loop.cancel_work(all_handles.back());
    EXPECT_FALSE(error);

    EXPECT_EQ(0, on_work_done_counter);

    ASSERT_EQ(io::StatusCode::OK, event_loop.run());

    EXPECT_EQ(0, on_work_done_counter);
}

TEST_F(EventLoopTest, work_cancel_during_loop_run) {
    const auto thread_pool_size = io::global::thread_pool_size();

    std::size_t on_work_done_counter = 0;

    io::EventLoop event_loop;

    auto dummy_work_callback = [](io::EventLoop&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    };

    auto cancelled_work_callback = [](io::EventLoop&) {
        FAIL() << "This callback should not be invoked!";
    };

    auto work_done_callback = [&](io::EventLoop&, const io::Error& error) {
        if (error) {
            EXPECT_EQ(io::StatusCode::OPERATION_CANCELED, error.code());
        } else {
            ++on_work_done_counter;
        }
    };

    std::vector<io::EventLoop::WorkHandle> all_handles;

    for (std::size_t i = 0 ; i < thread_pool_size; ++i) {
        const auto handle = event_loop.add_work(
            dummy_work_callback,
            work_done_callback
        );
        EXPECT_NE(io::EventLoop::INVALID_HANDLE, handle) << "i=" << i;
        all_handles.push_back(handle);
    }

    {
        const auto handle = event_loop.add_work(
            cancelled_work_callback,
            work_done_callback
        );
        EXPECT_NE(io::EventLoop::INVALID_HANDLE, handle);
        all_handles.push_back(handle);
    }

    // Here we do not want to use Timer because Timer is dependent on EventLoop and we want to
    // ensure in tests that EventLoop is working first. Need to wait for a while because work is not
    // started immediately. At least Ubuntu18.04 has this weird behavior.
    const auto start_time = std::chrono::high_resolution_clock::now();
    std::size_t each_loop_cycle_handle = 0;
    each_loop_cycle_handle = event_loop.schedule_call_on_each_loop_cycle([&](io::EventLoop& loop) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start_time).count() > 100) {

            for (std::size_t i = 0 ; i < thread_pool_size; ++i) {
                auto error = loop.cancel_work(all_handles[i]);
                EXPECT_TRUE(error);
                EXPECT_EQ(io::StatusCode::RESOURCE_BUSY_OR_LOCKED, error.code());
            }

            auto error = loop.cancel_work(all_handles.back());
            EXPECT_FALSE(error) << error.string();

            loop.stop_call_on_each_loop_cycle(each_loop_cycle_handle);
        }
    });

    EXPECT_EQ(0, on_work_done_counter);

    ASSERT_EQ(io::StatusCode::OK, event_loop.run());

    EXPECT_EQ(thread_pool_size, on_work_done_counter);
}

TEST_F(EventLoopTest, schedule_on_each_loop_cycle) {
    io::EventLoop loop;

    size_t counter = 0;
    size_t handle = io::EventLoop::INVALID_HANDLE;

    handle = loop.schedule_call_on_each_loop_cycle([&handle, &counter](io::EventLoop& loop) {
        ++counter;

        if (counter == 500) {
            loop.stop_call_on_each_loop_cycle(handle);
        }
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_EQ(500, counter);
}

TEST_F(EventLoopTest, stop_call_on_each_loop_cycle_with_invalid_data) {
    // Crash test
    io::EventLoop loop;
    loop.stop_call_on_each_loop_cycle(io::EventLoop::INVALID_HANDLE);
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(EventLoopTest, multiple_schedule_on_each_loop_cycle) {
    io::EventLoop loop;

    size_t counter_1 = 0;
    size_t handle_1 = io::EventLoop::INVALID_HANDLE;
    size_t counter_2 = 0;
    size_t handle_2 = io::EventLoop::INVALID_HANDLE;
    size_t counter_3 = 0;
    size_t handle_3 = io::EventLoop::INVALID_HANDLE;

    handle_1 = loop.schedule_call_on_each_loop_cycle([&handle_1, &counter_1](io::EventLoop& loop) {
        ++counter_1;

        if (counter_1 == 500) {
            loop.stop_call_on_each_loop_cycle(handle_1);
        }
    });

    handle_2 = loop.schedule_call_on_each_loop_cycle([&handle_2, &counter_2](io::EventLoop& loop) {
        ++counter_2;

        if (counter_2 == 200) {
            loop.stop_call_on_each_loop_cycle(handle_2);
        }
    });

    handle_3 = loop.schedule_call_on_each_loop_cycle([&handle_3, &counter_3](io::EventLoop& loop) {
        ++counter_3;

        if (counter_3 == 300) {
            loop.stop_call_on_each_loop_cycle(handle_3);
        }
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_EQ(500, counter_1);
    EXPECT_EQ(200, counter_2);
    EXPECT_EQ(300, counter_3);
}

TEST_F(EventLoopTest, is_running) {
    io::EventLoop event_loop;

    std::size_t callback_call_count = 0;

    std::size_t handle = io::EventLoop::INVALID_HANDLE;
    handle = event_loop.schedule_call_on_each_loop_cycle([&](io::EventLoop& loop) {
        EXPECT_TRUE(loop.is_running());
        loop.stop_call_on_each_loop_cycle(handle);
        ++callback_call_count;
    });

    EXPECT_EQ(0, callback_call_count);
    EXPECT_FALSE(event_loop.is_running());
    ASSERT_EQ(io::StatusCode::OK, event_loop.run());
    EXPECT_FALSE(event_loop.is_running());
    EXPECT_EQ(1, callback_call_count);
}

TEST_F(EventLoopTest, loop_in_thread) {
    std::mutex data_mutex;
    size_t combined_counter = 0;

    auto functor = [&combined_counter, &data_mutex]() {
        io::EventLoop event_loop;
        size_t counter = 0;

        const size_t handle = event_loop.schedule_call_on_each_loop_cycle([&](io::EventLoop& loop) {
            EXPECT_EQ(&event_loop, &loop);
            ++counter;

            if (counter == 200) {
                loop.stop_call_on_each_loop_cycle(handle);
            }
        });

        event_loop.run();

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
    loop.start_block_loop_from_exit();
    loop.start_block_loop_from_exit();
    loop.stop_block_loop_from_exit();
    loop.stop_block_loop_from_exit();
    loop.start_block_loop_from_exit();
    loop.stop_block_loop_from_exit();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(EventLoopTest, execute_on_loop_thread_from_main_thread) {
    io::EventLoop loop;

    auto main_thread_id = std::this_thread::get_id();
    int execute_on_loop_thread_call_counter = 0;

    loop.execute_on_loop_thread([&main_thread_id, &execute_on_loop_thread_call_counter](io::EventLoop& loop){
        ASSERT_EQ(main_thread_id, std::this_thread::get_id());
        ++execute_on_loop_thread_call_counter;
    });

    EXPECT_EQ(0, execute_on_loop_thread_call_counter);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(0, execute_on_loop_thread_call_counter);
    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_EQ(1, execute_on_loop_thread_call_counter);
}

TEST_F(EventLoopTest, execute_on_loop_thread_nested) {
    io::EventLoop loop;

    auto main_thread_id = std::this_thread::get_id();
    int execute_on_loop_thread_call_counter_1 = 0;
    int execute_on_loop_thread_call_counter_2 = 0;

    loop.execute_on_loop_thread([&](io::EventLoop&) {
        ASSERT_EQ(main_thread_id, std::this_thread::get_id());
        ++execute_on_loop_thread_call_counter_1;

        loop.execute_on_loop_thread([&](io::EventLoop&) {
            ASSERT_EQ(main_thread_id, std::this_thread::get_id());
            ++execute_on_loop_thread_call_counter_2;
        });
    });

    EXPECT_EQ(0, execute_on_loop_thread_call_counter_1);
    EXPECT_EQ(0, execute_on_loop_thread_call_counter_2);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(0, execute_on_loop_thread_call_counter_1);
    EXPECT_EQ(0, execute_on_loop_thread_call_counter_2);
    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_EQ(1, execute_on_loop_thread_call_counter_1);
    EXPECT_EQ(1, execute_on_loop_thread_call_counter_2);
}

TEST_F(EventLoopTest, execute_on_loop_thread_from_other_thread) {
    io::EventLoop loop;

    auto main_thread_id = std::this_thread::get_id();
    bool execute_on_loop_thread_called = false;

    loop.start_block_loop_from_exit(); // need to hold loop running

    std::thread thread([&loop, &execute_on_loop_thread_called, &main_thread_id](){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        loop.execute_on_loop_thread([&execute_on_loop_thread_called, &main_thread_id](io::EventLoop& loop){
            ASSERT_EQ(main_thread_id, std::this_thread::get_id());
            execute_on_loop_thread_called = true;
            loop.stop_block_loop_from_exit();
        });
    });

    io::ScopeExitGuard scope_guard([&thread](){
        thread.join();
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_TRUE(execute_on_loop_thread_called);
}

TEST_F(EventLoopTest, run_loop_several_times) {
    io::EventLoop loop;

    int counter_1 = 0;
    int counter_2 = 0;
    int counter_3 = 0;

    loop.execute_on_loop_thread([&](io::EventLoop&) {
        ++counter_1;
    });

    EXPECT_EQ(0, counter_1);
    EXPECT_EQ(0, counter_2);
    EXPECT_EQ(0, counter_3);

    ASSERT_EQ(io::StatusCode::OK, loop.run()); // run 1

    EXPECT_EQ(1, counter_1);
    EXPECT_EQ(0, counter_2);
    EXPECT_EQ(0, counter_3);

    loop.execute_on_loop_thread([&](io::EventLoop&) {
        ++counter_2;
    });

    EXPECT_EQ(1, counter_1);
    EXPECT_EQ(0, counter_2);
    EXPECT_EQ(0, counter_3);

    ASSERT_EQ(io::StatusCode::OK, loop.run()); // run 2

    EXPECT_EQ(1, counter_1);
    EXPECT_EQ(1, counter_2);
    EXPECT_EQ(0, counter_3);

    loop.execute_on_loop_thread([&](io::EventLoop&) {
        ++counter_3;
    });

    EXPECT_EQ(1, counter_1);
    EXPECT_EQ(1, counter_2);
    EXPECT_EQ(0, counter_3);

    ASSERT_EQ(io::StatusCode::OK, loop.run()); // run 3

    EXPECT_EQ(1, counter_1);
    EXPECT_EQ(1, counter_2);
    EXPECT_EQ(1, counter_3);
}

TEST_F(EventLoopTest, schedule_1_callback) {
    io::EventLoop loop;

    std::size_t callback_counter = 0;
    loop.schedule_callback([&](io::EventLoop&){
        callback_counter++;
    });

    EXPECT_EQ(0, callback_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, callback_counter);
}

TEST_F(EventLoopTest, schedule_callback_after_loop_run) {
    std::size_t callback_counter = 0;
    io::ScopeExitGuard scope_guard([&](){
        EXPECT_EQ(0, callback_counter);
    });

    io::EventLoop loop;

    EXPECT_EQ(0, callback_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    loop.schedule_callback([&](io::EventLoop&){
        callback_counter++;
    });

    EXPECT_EQ(0, callback_counter);
}

TEST_F(EventLoopTest, schedule_multiple_callbacks_parallel) {
    io::EventLoop loop;

    std::size_t callback_counter_1 = 0;
    std::size_t callback_counter_2 = 0;
    std::size_t callback_counter_3 = 0;

    auto callback_1 = [&](io::EventLoop&) {
        ++callback_counter_1;
    };

    auto callback_2 = [&](io::EventLoop&) {
        ++callback_counter_2;
    };

    auto callback_3 = [&](io::EventLoop&) {
        ++callback_counter_3;
    };

    loop.schedule_callback(callback_1);
    loop.schedule_callback(callback_2);
    loop.schedule_callback(callback_3);

    EXPECT_EQ(0, callback_counter_1);
    EXPECT_EQ(0, callback_counter_2);
    EXPECT_EQ(0, callback_counter_3);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, callback_counter_1);
    EXPECT_EQ(1, callback_counter_2);
    EXPECT_EQ(1, callback_counter_3);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, callback_counter_1);
    EXPECT_EQ(1, callback_counter_2);
    EXPECT_EQ(1, callback_counter_3);

    loop.schedule_callback(callback_1);
    loop.schedule_callback(callback_2);

    EXPECT_EQ(1, callback_counter_1);
    EXPECT_EQ(1, callback_counter_2);
    EXPECT_EQ(1, callback_counter_3);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(2, callback_counter_1);
    EXPECT_EQ(2, callback_counter_2);
    EXPECT_EQ(1, callback_counter_3);
}

TEST_F(EventLoopTest, schedule_multiple_callbacks_sequential) {
    io::EventLoop loop;

    std::size_t callback_counter_1 = 0;
    std::size_t callback_counter_2 = 0;
    std::size_t callback_counter_3 = 0;

    auto callback_3 = [&](io::EventLoop&) {
        ++callback_counter_3;
    };

    auto callback_2 = [&](io::EventLoop&) {
        ++callback_counter_2;
        loop.schedule_callback(callback_3);
    };

    auto callback_1 = [&](io::EventLoop&) {
        ++callback_counter_1;

        loop.schedule_callback(callback_2);
    };

    loop.schedule_callback(callback_1);

    EXPECT_EQ(0, callback_counter_1);
    EXPECT_EQ(0, callback_counter_2);
    EXPECT_EQ(0, callback_counter_3);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, callback_counter_1);
    EXPECT_EQ(1, callback_counter_2);
    EXPECT_EQ(1, callback_counter_3);
}

TEST_F(EventLoopTest, create_loop_in_one_thread_and_run_in_another) {
    io::EventLoop loop;

    std::size_t callback_counter = 0;

    std::thread([&]() {
        loop.execute_on_loop_thread([&](io::EventLoop&) {
            ++callback_counter;
        });

        EXPECT_EQ(0, callback_counter);

        ASSERT_FALSE(loop.run());
    }).join();

    EXPECT_EQ(1, callback_counter);
}

TEST_F(EventLoopTest, stop_signal_without_handler) {
    // Test description: crash test
    io::EventLoop loop;

    loop.remove_signal_handler(io::EventLoop::Signal::HUP);

    ASSERT_FALSE(loop.run());
}

TEST_F(EventLoopTest, signal_no_callback) {
    io::EventLoop loop;

    auto error = loop.add_signal_handler(io::EventLoop::Signal::HUP,
        nullptr
    );
    ASSERT_TRUE(error);
    EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());

    ASSERT_FALSE(loop.run());
}

// lldb, to not stop on signal type: process handle SIGHUP -s false
TEST_F(EventLoopTest, signal_repeating) {
#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    io::EventLoop loop;

    std::size_t callback_counter = 0;

    auto error = loop.add_signal_handler(io::EventLoop::Signal::HUP,
        [&](io::EventLoop&, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++callback_counter;
            loop.remove_signal_handler(io::EventLoop::Signal::HUP);
        }
    );
    ASSERT_FALSE(error) << error;

    const auto start_time = std::chrono::high_resolution_clock::now();
    std::size_t each_loop_cycle_handle = 0;
    each_loop_cycle_handle = loop.schedule_call_on_each_loop_cycle([&](io::EventLoop& loop) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start_time).count() > 50) {
            raise(SIGHUP);
            loop.stop_call_on_each_loop_cycle(each_loop_cycle_handle);
        }
    });

    EXPECT_EQ(0, callback_counter);

    ASSERT_FALSE(loop.run());

    EXPECT_EQ(1, callback_counter);
#else
    IO_TEST_SKIP();
#endif
}

TEST_F(EventLoopTest, signal_once) {
#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    io::EventLoop loop;

    std::size_t callback_counter = 0;

    const auto error = loop.handle_signal_once(io::EventLoop::Signal::HUP,
        [&](io::EventLoop&, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++callback_counter;
        }
    );
    ASSERT_FALSE(error) << error;

    const auto start_time = std::chrono::high_resolution_clock::now();
    std::size_t each_loop_cycle_handle = 0;
    each_loop_cycle_handle = loop.schedule_call_on_each_loop_cycle([&](io::EventLoop& loop) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start_time).count() > 50) {
            raise(SIGHUP);
            loop.stop_call_on_each_loop_cycle(each_loop_cycle_handle);
        }
    });

    EXPECT_EQ(0, callback_counter);

    ASSERT_FALSE(loop.run());

    EXPECT_EQ(1, callback_counter);
#else
    IO_TEST_SKIP();
#endif
}

TEST_F(EventLoopTest, signal_schedule_once_and_cancel) {
    io::EventLoop loop;

    std::size_t callback_counter = 0;

    const auto error = loop.handle_signal_once(io::EventLoop::Signal::HUP,
        [&](io::EventLoop&, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++callback_counter;
        }
    );
    ASSERT_FALSE(error) << error;

    loop.remove_signal_handler(io::EventLoop::Signal::HUP);

    EXPECT_EQ(0, callback_counter);

    ASSERT_FALSE(loop.run());

    EXPECT_EQ(0, callback_counter);
}

TEST_F(EventLoopTest, signal_once_schedule_cancel_and_scgedule_again) {
#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    io::EventLoop loop;

    std::size_t callback_counter_1 = 0;
    std::size_t callback_counter_2 = 0;

    auto error = loop.handle_signal_once(io::EventLoop::Signal::HUP,
        [&](io::EventLoop&, const io::Error& error) {
            // Should be never called
            EXPECT_FALSE(error) << error;
            ++callback_counter_1;
        }
    );
    ASSERT_FALSE(error) << error;

    loop.remove_signal_handler(io::EventLoop::Signal::HUP);

    error = loop.handle_signal_once(io::EventLoop::Signal::HUP,
        [&](io::EventLoop&, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++callback_counter_2;
        }
    );
    ASSERT_FALSE(error) << error;

    const auto start_time = std::chrono::high_resolution_clock::now();
    std::size_t each_loop_cycle_handle = 0;
    each_loop_cycle_handle = loop.schedule_call_on_each_loop_cycle([&](io::EventLoop& loop) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start_time).count() > 50) {
            raise(SIGHUP);
            loop.stop_call_on_each_loop_cycle(each_loop_cycle_handle);
        }
    });

    EXPECT_EQ(0, callback_counter_1);
    EXPECT_EQ(0, callback_counter_2);

    ASSERT_FALSE(loop.run());

    EXPECT_EQ(0, callback_counter_1);
    EXPECT_EQ(1, callback_counter_2);
#else
    IO_TEST_SKIP();
#endif
}

TEST_F(EventLoopTest, signal_force_out_one_callback_with_another) {
#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    io::EventLoop loop;

    std::size_t callback_counter_1 = 0;
    std::size_t callback_counter_2 = 0;

    auto error = loop.add_signal_handler(io::EventLoop::Signal::HUP,
        [&](io::EventLoop&, const io::Error& error) {
            // Should be never called
            EXPECT_FALSE(error) << error;
            ++callback_counter_1;
        }
    );
    ASSERT_FALSE(error) << error;

    error = loop.add_signal_handler(io::EventLoop::Signal::HUP,
        [&](io::EventLoop&, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++callback_counter_2;
            loop.remove_signal_handler(io::EventLoop::Signal::HUP);
        }
    );
    ASSERT_FALSE(error) << error;

    const auto start_time = std::chrono::high_resolution_clock::now();
    std::size_t each_loop_cycle_handle = 0;
    each_loop_cycle_handle = loop.schedule_call_on_each_loop_cycle([&](io::EventLoop& loop) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start_time).count() > 50) {
            raise(SIGHUP);
            loop.stop_call_on_each_loop_cycle(each_loop_cycle_handle);
        }
    });

    EXPECT_EQ(0, callback_counter_1);
    EXPECT_EQ(0, callback_counter_2);

    ASSERT_FALSE(loop.run());

    EXPECT_EQ(0, callback_counter_1);
    EXPECT_EQ(1, callback_counter_2);
#else
    IO_TEST_SKIP();
#endif
}

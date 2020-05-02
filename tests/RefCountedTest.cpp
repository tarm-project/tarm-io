/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "io/RefCounted.h"

struct RefCountedTest : public testing::Test,
                        public LogRedirector {
};

TEST_F(RefCountedTest, all_in_one_test) {
    io::EventLoop loop;

    std::size_t on_schedule_removal_call_count = 0;

    auto ref_counted = new io::RefCounted(loop);
    ref_counted->set_on_schedule_removal([&on_schedule_removal_call_count](const io::Removable&) {
        ++on_schedule_removal_call_count;
    });

    EXPECT_EQ(1, ref_counted->ref_count());

    int idle_counter = 0;
    std::size_t handle = loop.schedule_call_on_each_loop_cycle([&](){
        switch (idle_counter) {
            case 0: {
                EXPECT_EQ(1, ref_counted->ref_count());
                EXPECT_EQ(0, on_schedule_removal_call_count);
                ref_counted->ref();
                EXPECT_EQ(2, ref_counted->ref_count());
            }
            break;

            case 1: {
                EXPECT_EQ(0, on_schedule_removal_call_count);
                EXPECT_EQ(2, ref_counted->ref_count());
                ref_counted->unref();
                EXPECT_EQ(1, ref_counted->ref_count());
            }
            break;

            case 2: {
                EXPECT_EQ(0, on_schedule_removal_call_count);
                EXPECT_EQ(1, ref_counted->ref_count());
                ref_counted->unref();
                EXPECT_EQ(0, ref_counted->ref_count());
                ref_counted->unref();
                EXPECT_EQ(0, ref_counted->ref_count());
                ref_counted->unref();
                EXPECT_EQ(0, ref_counted->ref_count());
            }
            break;

            default:
                EXPECT_EQ(1, on_schedule_removal_call_count);
                loop.stop_call_on_each_loop_cycle(handle);
                break;
        };
        idle_counter++;
    });

    EXPECT_EQ(0, on_schedule_removal_call_count);

    ASSERT_EQ(0, loop.run());

    EXPECT_EQ(1, on_schedule_removal_call_count);

    /*
    auto td = new TestRemovable(loop, g_disposed_2);
    td->schedule_removal();
    loop.add_work([](){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });

    ASSERT_EQ(0, loop.run());

    ASSERT_TRUE(g_disposed_2);
    */
}

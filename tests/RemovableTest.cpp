/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "io/Removable.h"

#include <chrono>
#include <thread>

struct RemovableTest : public testing::Test,
                       public LogRedirector {
};

namespace {

bool g_disposed = false;

class TestRemovable : public io::Removable {
public:
    TestRemovable(io::EventLoop& loop, bool& disposed_flag) :
        Removable(loop),
        m_disposed_flag(disposed_flag) {
    }

protected:
    ~TestRemovable() {
        m_disposed_flag = true;
    }

private:
    bool& m_disposed_flag;
};

} // namespace

TEST_F(RemovableTest, schedule_removal) {
    io::EventLoop loop;

    auto td = new TestRemovable(loop, g_disposed);
    EXPECT_FALSE(td->is_removal_scheduled());
    td->schedule_removal();
    EXPECT_TRUE(td->is_removal_scheduled());

    loop.add_work([](io::EventLoop&){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    ASSERT_TRUE(g_disposed);
}

TEST_F(RemovableTest, callback) {
    io::EventLoop loop;

    std::size_t callback_1_counter = 0;
    std::size_t callback_2_counter = 0;

    auto removalbe = new io::Removable(loop);
    auto callback_1 = [&](const io::Removable& r) {
        EXPECT_EQ(removalbe, &r);
        ++callback_1_counter;

        // callback 2
        removalbe->set_on_schedule_removal([&](const io::Removable& r) {
            ++callback_2_counter; // should not be called
        });
    };

    removalbe->set_on_schedule_removal(callback_1);
    removalbe->schedule_removal();

    EXPECT_EQ(0, callback_1_counter);
    EXPECT_EQ(0, callback_2_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, callback_1_counter);
    EXPECT_EQ(0, callback_2_counter);
}

TEST_F(RemovableTest, change_callback_after_schedule_removal) {
    io::EventLoop loop;

    std::size_t callback_1_counter = 0;
    std::size_t callback_2_counter = 0;

    auto callback_1 = [&](const io::Removable& r) {
        ++callback_1_counter;
    };

    auto callback_2 = [&](const io::Removable& r) {
        ++callback_2_counter;
    };

    auto removalbe = new io::Removable(loop);
    removalbe->set_on_schedule_removal(callback_1);
    removalbe->schedule_removal();
    removalbe->set_on_schedule_removal(callback_2);

    EXPECT_EQ(0, callback_1_counter);
    EXPECT_EQ(0, callback_2_counter);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(0, callback_1_counter);
    EXPECT_EQ(1, callback_2_counter);
}

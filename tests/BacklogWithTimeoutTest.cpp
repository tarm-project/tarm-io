/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "io/BacklogWithTimeout.h"

#include "UTCommon.h"

#include <uv.h>

#include <memory>

class FakeTimer;

struct BacklogWithTimeoutTest : public testing::Test,
                                public LogRedirector {
public:
    void add_fake_timer(FakeTimer* timer) {
        m_fake_timers.push_back(timer);
        m_time_getter_count = 0;
    }

    // advance monothonic clock and execute times if any
    void advance_clock(std::uint64_t time_ms);

    static void add_time_getter_count() {
        ++m_time_getter_count;
    }

protected:
    BacklogWithTimeoutTest() {
        reset_fake_monothonic_clock();
    }

    static void reset_fake_monothonic_clock(std::uint64_t value = 0) {
        m_fake_clock = value;
    }

    static std::uint64_t fake_monothonic_clock() {
        return m_fake_clock;
    }

    ~BacklogWithTimeoutTest();

    static std::size_t time_getter_count() {
        return m_time_getter_count;
    }

private:
    static std::uint64_t m_fake_clock;
    std::vector<FakeTimer*> m_fake_timers;

    static std::size_t m_time_getter_count;
};

std::uint64_t BacklogWithTimeoutTest::m_fake_clock = 0;
std::size_t BacklogWithTimeoutTest::m_time_getter_count = 0;

namespace {

struct TestItem {
    TestItem(std::size_t id_) :
        id(id_) {}

    std::size_t id = 0;
    std::uint64_t time = 0;

    std::uint64_t time_getter() const {
        BacklogWithTimeoutTest::add_time_getter_count();
        return time;
    }
};

bool operator==(const TestItem& item1, const TestItem& item2) {
    return item1.id == item2.id;
}

} // namespace

class FakeLoop : public ::io::UserDataHolder {
public:
    FakeLoop() = default;
};

class FakeTimer : public ::io::UserDataHolder {
public:
    using DefaultDelete = std::function<void(FakeTimer*)>;

    static DefaultDelete default_delete() {
        return [](FakeTimer* ft) {
            // Do nothing. Fake timers lifetime is managed by test suite
            ft->m_stopped = true;
        };
    }

    using Callback = std::function<void(FakeTimer&)>;

    IO_FORBID_COPY(FakeTimer);
    IO_FORBID_MOVE(FakeTimer);

    FakeTimer(FakeLoop& loop) {
        auto& test_suite = *reinterpret_cast<BacklogWithTimeoutTest*>(loop.user_data());
        test_suite.add_fake_timer(this);
    }

    ~FakeTimer() {
    }

    void start(uint64_t timeout_ms, uint64_t repeat_ms, Callback callback) {
        assert(timeout_ms == repeat_ms);
        assert(callback != nullptr);
        m_timeout_ms = timeout_ms;
        m_callback = callback;
    }

    void start(uint64_t timeout_ms, Callback callback) {
        assert(false && "should not be called");
    }

    void stop() {
        m_stopped = true;
    }

    uint64_t timeout_ms() const {
        return m_timeout_ms;
    }

    void advance(uint64_t time_ms) {
        if (m_stopped) {
            return;
        }

        for (uint64_t i = 0; i < time_ms; ++i) {
            ++m_current_time_ms;
            if (m_stopped) {
                break;
            }

            if (m_current_time_ms >= m_timeout_ms) {
                m_callback(*this);
                m_current_time_ms = 0;
            }
        }
    }

private:
    uint64_t m_timeout_ms = 0;
    Callback m_callback = nullptr;
    uint64_t m_current_time_ms = 0;

    bool m_stopped = false;
};

void BacklogWithTimeoutTest::advance_clock(std::uint64_t time_ms) {
    for (std::uint64_t i = 0; i < time_ms; ++i) {
        m_fake_clock += std::uint64_t(1000000);

        for(auto& timer : m_fake_timers) {
            timer->advance(1);
        }
    }
}

BacklogWithTimeoutTest::~BacklogWithTimeoutTest() {
    for (auto& t : m_fake_timers) {
        delete t;
    }
}

// --- fake loop and timer ---

TEST_F(BacklogWithTimeoutTest, 1_element) {
    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer>&, const TestItem& item) {
        EXPECT_EQ(0, item.id);
        ++expired_counter;
    };

    FakeLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer> backlog(
        loop, 250, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    TestItem item_1(0);
    EXPECT_TRUE(backlog.add_item(item_1));

    EXPECT_EQ(0, expired_counter);

    advance_clock(250);

    EXPECT_EQ(1, expired_counter);
}

TEST_F(BacklogWithTimeoutTest, multiple_elements_at_the_same_time) {
    const std::size_t ELEMENTS_COUNT = 256;

    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem*, FakeLoop, FakeTimer>&, TestItem* const& item) {
        EXPECT_EQ(expired_counter, item->id);
        ++expired_counter;
    };

    auto time_getter = [&](TestItem* const& item) -> std::uint64_t {
        return item->time_getter();
    };

    FakeLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem*, FakeLoop, FakeTimer> backlog(
        loop, 250, on_expired, time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    std::vector<std::unique_ptr<TestItem>> items;
    for (std::size_t i = 0; i < ELEMENTS_COUNT; ++i) {
        items.emplace_back(new TestItem(i));
        EXPECT_TRUE(backlog.add_item(items.back().get()));
    }

    EXPECT_EQ(0, expired_counter);

    advance_clock(250);

    EXPECT_EQ(ELEMENTS_COUNT, expired_counter);
}

TEST_F(BacklogWithTimeoutTest, multiple_elements_with_time_near_timeout) {
    // Note: in this test we check that elements which are updated recently will use the largest available timer interval

    const std::uint64_t START_TIME = 250 * std::uint64_t(10000000);
    reset_fake_monothonic_clock(START_TIME);

    const std::size_t ELEMENTS_COUNT = 10;

    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer>&, const TestItem& item) {
        EXPECT_EQ(expired_counter, item.id);
        ++expired_counter;
    };

    FakeLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer> backlog(
        loop, 2500, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    for (std::size_t i = 0; i < ELEMENTS_COUNT; ++i) {
        TestItem item(i);
        item.time = START_TIME - i * 10000;
        ASSERT_TRUE(backlog.add_item(item)) << i;
    }

    advance_clock(5000);

    EXPECT_EQ(ELEMENTS_COUNT, expired_counter);

    // Time getter is called 2 times: 1 time during adding element and 2 time on timeout before expired callback
    EXPECT_EQ(ELEMENTS_COUNT * 2, this->time_getter_count());
}

TEST_F(BacklogWithTimeoutTest, multiple_elements_in_distinct_time) {
    reset_fake_monothonic_clock(250 * std::uint64_t(10000000));

    const std::size_t ELEMENTS_COUNT = 250;

    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer>&, const TestItem& item) {
        EXPECT_EQ(expired_counter, item.id);
        ++expired_counter;
    };

    FakeLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer> backlog(
        loop, 2500, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    for (std::size_t i = 0; i < ELEMENTS_COUNT; ++i) {
        TestItem item(i);
        item.time = i * 10000000;
        ASSERT_TRUE(backlog.add_item(item)) << i;
    }

    // Item with index 0 is expired durin adding
    EXPECT_EQ(1, expired_counter);

    advance_clock(5000);

    EXPECT_EQ(ELEMENTS_COUNT, expired_counter);
}


TEST_F(BacklogWithTimeoutTest, 1ms_timeout) {
    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer>&, const TestItem& item) {
        EXPECT_EQ(0, item.id);
        ++expired_counter;
    };

    FakeLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer> backlog(
        loop, 1, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    TestItem item_1(0);
    EXPECT_TRUE(backlog.add_item(item_1));

    EXPECT_EQ(0, expired_counter);

    advance_clock(1);

    EXPECT_EQ(1, expired_counter);
}

TEST_F(BacklogWithTimeoutTest, discard_item_from_future) {
    auto on_expired = [&](io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer>&, const TestItem& item) {
        EXPECT_TRUE(false); // should not be called
    };

    auto time_getter = [&](const TestItem& item) -> std::uint64_t {
        return item.time_getter();
    };

    FakeLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer> backlog(
        loop, 1, on_expired, time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    TestItem item_1(0);
    item_1.time = 100500; // time from the future
    EXPECT_FALSE(backlog.add_item(item_1));
}

TEST_F(BacklogWithTimeoutTest, stop_on_first_item) {
    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<std::shared_ptr<TestItem>, FakeLoop, FakeTimer>& backlog, const std::shared_ptr<TestItem>& item) {
        EXPECT_EQ(0, item->id);
        ++expired_counter;
        backlog.stop();
    };

    FakeLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<std::shared_ptr<TestItem>, FakeLoop, FakeTimer> backlog(
        loop, 1, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    EXPECT_TRUE(backlog.add_item(std::make_shared<TestItem>(0)));
    EXPECT_TRUE(backlog.add_item(std::make_shared<TestItem>(1)));

    EXPECT_EQ(0, expired_counter);

    advance_clock(100);

    EXPECT_EQ(1, expired_counter);
}

TEST_F(BacklogWithTimeoutTest, update_item_time) {
    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<std::shared_ptr<TestItem>, FakeLoop, FakeTimer>& backlog, const std::shared_ptr<TestItem>& item) {
        ++expired_counter;
    };

    FakeLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<std::shared_ptr<TestItem>, FakeLoop, FakeTimer> backlog(
        loop, 100, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    auto item = std::make_shared<TestItem>(0);
    EXPECT_TRUE(backlog.add_item(item));

    EXPECT_EQ(0, expired_counter);

    advance_clock(90);

    EXPECT_EQ(0, expired_counter);

    item->time = 90 * 1000000;
    advance_clock(90);

    EXPECT_EQ(0, expired_counter);

    item->time = 2 * 90 * 1000000;
    advance_clock(90);

    EXPECT_EQ(0, expired_counter);

    advance_clock(90);

    EXPECT_EQ(1, expired_counter);
}

// Used for performance analysis
TEST_F(BacklogWithTimeoutTest, huge_number_of_items) {
    const std::size_t ELEMENTS_COUNT = 1000000 * 2;

    const std::uint64_t START_TIME = 250 * 1000000;
    reset_fake_monothonic_clock(START_TIME);

    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer>&, const TestItem& item) {
        ++expired_counter;
    };

    FakeLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer> backlog(
        loop, 250, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    for (std::size_t i = 0; i < ELEMENTS_COUNT; ++i) {
        TestItem item(i);
        item.time = START_TIME - i * 100;
        ASSERT_TRUE(backlog.add_item(item)) << i;
    }

    EXPECT_EQ(0, expired_counter);

    for (std::size_t i = 0; i < 500; ++i) {
        advance_clock(1);
    }

    EXPECT_EQ(ELEMENTS_COUNT, expired_counter);
}

TEST_F(BacklogWithTimeoutTest, remove_1_element) {
    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer>&, const TestItem& item) {
        ++expired_counter;
    };

    FakeLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer> backlog(
        loop, 250, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    TestItem item_1(0);
    EXPECT_TRUE(backlog.add_item(item_1));

    EXPECT_EQ(0, expired_counter);

    advance_clock(200);

    EXPECT_EQ(0, expired_counter);

    EXPECT_TRUE(backlog.remove_item(item_1));

    EXPECT_EQ(0, expired_counter);

    advance_clock(50);

    EXPECT_EQ(0, expired_counter);
}

TEST_F(BacklogWithTimeoutTest, remove_multiple_items) {
    const std::size_t ELEMENTS_COUNT = 1000;

    const std::uint64_t START_TIME = 250 * 1000000;
    reset_fake_monothonic_clock(START_TIME);

    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer>&, const TestItem& item) {
        ++expired_counter;
        EXPECT_NE(0, item.id % 2);
    };

    FakeLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer> backlog(
        loop, 250, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    for (std::size_t i = 0; i < ELEMENTS_COUNT; ++i) {
        TestItem item(i);
        item.time = START_TIME - i * 1000000 / 4;
        ASSERT_TRUE(backlog.add_item(item)) << i;
    }

    for (std::size_t i = 0; i < ELEMENTS_COUNT; ++i) {
        if (i % 2 == 0) {
            TestItem item(i);
            item.time = START_TIME - i * 1000000 / 4;
            backlog.remove_item(item);
        }
    }

    EXPECT_EQ(0, expired_counter);

    for (std::size_t i = 0; i < 500; ++i) {
        advance_clock(1);
    }

    EXPECT_EQ(ELEMENTS_COUNT / 2, expired_counter);
}

TEST_F(BacklogWithTimeoutTest, move_constructor) {
    using BacklogType = io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer>;

    BacklogType* backlog_ptr = nullptr;

    std::size_t expired_counter = 0;
    auto on_expired = [&](BacklogType& backlog, const TestItem& item) {
        EXPECT_EQ(&backlog, backlog_ptr);
        EXPECT_EQ(0, item.id);
        ++expired_counter;
    };

    FakeLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer> backlog(
        loop, 250, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    TestItem item_1(0);
    EXPECT_TRUE(backlog.add_item(item_1));

    auto backlog2 = std::move(backlog);
    backlog_ptr = &backlog2;

    EXPECT_EQ(0, expired_counter);

    advance_clock(250);

    EXPECT_EQ(1, expired_counter);
}

TEST_F(BacklogWithTimeoutTest, move_assignment) {
    using BacklogType = io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer>;

    BacklogType* backlog_ptr = nullptr;

    std::size_t expired_counter = 0;
    auto on_expired = [&](BacklogType& backlog, const TestItem& item) {
        EXPECT_EQ(&backlog, backlog_ptr);
        EXPECT_EQ(0, item.id);
        ++expired_counter;
    };

    FakeLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer> backlog(
        loop, 250, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    TestItem item_1(0);
    EXPECT_TRUE(backlog.add_item(item_1));

    // on_expired_2 will not be called because it will be replaced by moved callback from other backlog
    std::size_t expired_counter_2 = 0;
    auto on_expired_2 = [&](BacklogType& backlog, const TestItem& item) {
        ++expired_counter_2;
    };
    io::BacklogWithTimeout<TestItem, FakeLoop, FakeTimer> backlog2(
        loop, 500, on_expired_2, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    backlog2 = std::move(backlog);
    backlog_ptr = &backlog2;

    EXPECT_EQ(0, expired_counter);
    EXPECT_EQ(0, expired_counter_2);

    advance_clock(250);

    EXPECT_EQ(1, expired_counter);
    EXPECT_EQ(0, expired_counter_2);
}

// --- real loop and timer ---

// TODO: fixme (uv_hrtime is taken from libuv but tests are not linked with)

/*
TEST_F(BacklogWithTimeoutTest, with_real_time_1_item) {
    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem>& backlog, const TestItem& item) {
        ++expired_counter;
        backlog.stop();
    };

    auto time_getter = [&](const TestItem& item) -> std::uint64_t {
        return item.time_getter();
    };

    io::EventLoop loop;
    io::BacklogWithTimeout<TestItem> backlog(loop, 1, on_expired, time_getter, &uv_hrtime);

    TestItem item_1(0);
    item_1.time = uv_hrtime();
    EXPECT_TRUE(backlog.add_item(item_1));

    EXPECT_EQ(0, expired_counter);

    EXPECT_EQ(0, loop.run());

    EXPECT_EQ(1, expired_counter);
}

*/

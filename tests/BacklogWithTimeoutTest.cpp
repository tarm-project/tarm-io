#include "io/BacklogWithTimeout.h"

#include "UTCommon.h"

#include <memory>


//namespace {
class FakeTimer;
//} // namespace


struct BacklogWithTimeoutTest : public testing::Test,
                                public LogRedirector {
public:
    void add_fake_timer(FakeTimer* timer) {
        m_fake_timers.push_back(timer);
    }

    // advance monothonic clock and execute times if any
    void advance_clock(std::uint64_t time_ms);

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

private:
    static std::uint64_t m_fake_clock;
    std::vector<FakeTimer*> m_fake_timers;
};

std::uint64_t BacklogWithTimeoutTest::m_fake_clock = 0;

namespace {

struct TestItem {
    TestItem(int id_) :
        id(id_) {}

    int id = 0;
    std::uint64_t time = 0;

    std::uint64_t time_getter() const {
        return time;
    }
};

} // namespace

class FakeTimer : public ::io::UserDataHolder {
public:
    using Callback = std::function<void(FakeTimer&)>;

    // TODO: this could be done with macros
    FakeTimer(const FakeTimer&) = delete;
    FakeTimer& operator=(const FakeTimer&) = delete;

    FakeTimer(FakeTimer&&) = default;
    FakeTimer& operator=(FakeTimer&&) = default;

    FakeTimer(io::EventLoop& loop) {
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
    }

    uint64_t timeout_ms() const {
        return m_timeout_ms;
    }

    void advance(uint64_t time_ms) {
        for (uint64_t i = 0; i < time_ms; ++i) {
            ++m_current_time_ms;
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
};

void BacklogWithTimeoutTest::advance_clock(std::uint64_t time_ms) {
    for (std::uint64_t i = 0; i < time_ms; ++i) {
        m_fake_clock += std::uint64_t(1000000);

        for(auto& timer : m_fake_timers) {
            timer->advance(1);
        }
    }
}

TEST_F(BacklogWithTimeoutTest, 1_element) {
    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem, FakeTimer>&, const TestItem& item) {
        EXPECT_EQ(0, item.id);
        ++expired_counter;
    };

    io::EventLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeTimer> backlog(
        loop, 250, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    TestItem item_1(0);
    EXPECT_TRUE(backlog.add_item(&item_1));

    EXPECT_EQ(0, expired_counter);

    advance_clock(250);

    EXPECT_EQ(1, expired_counter);

    EXPECT_EQ(0, loop.run());
}

TEST_F(BacklogWithTimeoutTest, multiple_elements_at_the_same_time) {
    const std::size_t ELEMENTS_COUNT = 256;

    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem, FakeTimer>&, const TestItem& item) {
        EXPECT_EQ(expired_counter, item.id);
        ++expired_counter;
    };

    io::EventLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeTimer> backlog(
        loop, 250, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    std::vector<std::unique_ptr<TestItem>> items;
    for (std::size_t i = 0; i < ELEMENTS_COUNT; ++i) {
        items.emplace_back(new TestItem(i));
        EXPECT_TRUE(backlog.add_item(items.back().get()));
    }

    EXPECT_EQ(0, expired_counter);

    advance_clock(250);

    EXPECT_EQ(ELEMENTS_COUNT, expired_counter);

    EXPECT_EQ(0, loop.run());
}

TEST_F(BacklogWithTimeoutTest, multiple_elements_in_distinct_time) {
    reset_fake_monothonic_clock(250 * 1000000);

    const std::size_t ELEMENTS_COUNT = 250;

    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem, FakeTimer>&, const TestItem& item) {
        EXPECT_EQ(expired_counter, item.id);
        ++expired_counter;
    };

    io::EventLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeTimer> backlog(
        loop, 250, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    std::vector<std::unique_ptr<TestItem>> items;
    for (std::size_t i = 0; i < ELEMENTS_COUNT; ++i) {
        items.emplace_back(new TestItem(i));
        items.back()->time = i * 1000000;
        ASSERT_TRUE(backlog.add_item(items.back().get())) << i;
    }

    // Item with index 0 is expired durin adding
    EXPECT_EQ(1, expired_counter);

    advance_clock(500);

    EXPECT_EQ(ELEMENTS_COUNT, expired_counter);

    EXPECT_EQ(0, loop.run());
}


TEST_F(BacklogWithTimeoutTest, 1ms_timeout) {
    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem, FakeTimer>&, const TestItem& item) {
        EXPECT_EQ(0, item.id);
        ++expired_counter;
    };

    io::EventLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeTimer> backlog(
        loop, 1, on_expired, &TestItem::time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    TestItem item_1(0);
    EXPECT_TRUE(backlog.add_item(&item_1));

    EXPECT_EQ(0, expired_counter);

    advance_clock(1);

    EXPECT_EQ(1, expired_counter);

    EXPECT_EQ(0, loop.run());
}

TEST_F(BacklogWithTimeoutTest, discard_item_from_future) {
    auto on_expired = [&](io::BacklogWithTimeout<TestItem, FakeTimer>&, const TestItem& item) {
        EXPECT_TRUE(false); // should not be called
    };

    auto time_getter = [&](const TestItem& item) -> std::uint64_t {
        return item.time_getter();
    };

    io::EventLoop loop;
    loop.set_user_data(this);
    io::BacklogWithTimeout<TestItem, FakeTimer> backlog(
        loop, 1, on_expired, time_getter, &BacklogWithTimeoutTest::fake_monothonic_clock);

    TestItem item_1(0);
    item_1.time = 100500; // time from the future
    EXPECT_FALSE(backlog.add_item(&item_1));

    EXPECT_EQ(0, loop.run());
}

TEST_F(BacklogWithTimeoutTest, with_real_time) {
    std::size_t expired_counter = 0;
    auto on_expired = [&](io::BacklogWithTimeout<TestItem>& backlog, const TestItem& item) {
        backlog.stop();
        ++expired_counter;
    };

    auto time_getter = [&](const TestItem& item) -> std::uint64_t {
        return item.time_getter();
    };

    io::EventLoop loop;
    io::BacklogWithTimeout<TestItem> backlog(loop, 1, on_expired, time_getter);

    TestItem item_1(0);
    item_1.time = uv_hrtime();
    EXPECT_TRUE(backlog.add_item(&item_1));

    EXPECT_EQ(0, expired_counter);

    EXPECT_EQ(0, loop.run());

    EXPECT_EQ(1, expired_counter);
}

#include "UTCommon.h"

#include "io/Removable.h"

#include <chrono>
#include <thread>

struct RemovableTest : public testing::Test,
                       public LogRedirector {
};

namespace {

bool g_disposed_1 = false;
bool g_disposed_2 = false;

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

TEST_F(RemovableTest, DISABLED_disposed_implicitly) {
    io::EventLoop loop;

    auto td = new TestRemovable(loop, g_disposed_1);

    ASSERT_EQ(0, loop.run());

    ASSERT_TRUE(g_disposed_1);
}

TEST_F(RemovableTest, disposed_explicitly) {
    io::EventLoop loop;

    auto td = new TestRemovable(loop, g_disposed_2);
    td->schedule_removal();
    loop.add_work([](){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });

    ASSERT_EQ(0, loop.run());

    ASSERT_TRUE(g_disposed_2);
}
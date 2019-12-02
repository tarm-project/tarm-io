#include "UTCommon.h"

#include "io/UserDataHolder.h"

#include <vector>

struct UserDataHolderTest : public testing::Test,
                            public LogRedirector {

};

TEST_F(UserDataHolderTest, default_constructor) {
    io::UserDataHolder holder;

    ASSERT_EQ(nullptr, holder.user_data());
}

TEST_F(UserDataHolderTest, accessor) {
    int value = 42;

    io::UserDataHolder holder;
    holder.set_user_data(&value);

    ASSERT_EQ(&value, holder.user_data());
}

TEST_F(UserDataHolderTest, move_1) {
    io::UserDataHolder holder;
    holder.set_user_data(reinterpret_cast<void*>(42));

    io::UserDataHolder holder_moved = std::move(holder);
    EXPECT_EQ(42, reinterpret_cast<std::size_t>(holder_moved.user_data()));
}

TEST_F(UserDataHolderTest, move_2) {
    const std::size_t COUNT = 1024;

    std::vector<io::UserDataHolder> holders;
    for (std::size_t i = 0; i < COUNT; ++i) {
        holders.emplace_back();
        holders.back().set_user_data(reinterpret_cast<void*>(i));
    }

    for (std::size_t i = 0; i < COUNT; ++i) {
        EXPECT_EQ(i, reinterpret_cast<std::size_t>(holders[i].user_data()));
    }
}

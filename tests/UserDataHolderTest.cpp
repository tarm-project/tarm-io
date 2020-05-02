/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

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

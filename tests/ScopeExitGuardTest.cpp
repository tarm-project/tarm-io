/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "ScopeExitGuard.h"

struct ScopeExitGuardTest : public testing::Test,
                            public LogRedirector {

};

TEST_F(ScopeExitGuardTest, callback_is_called) {
    bool callback_called_1 = false;
    bool callback_called_2 = false;

    io::ScopeExitGuard guard_1([&]() {
        ASSERT_TRUE(callback_called_2);
        callback_called_1 = true;
    });

    io::ScopeExitGuard guard_2([&]() {
        ASSERT_FALSE(callback_called_1);
        callback_called_2 = true;
    });

    ASSERT_FALSE(callback_called_1);
    ASSERT_FALSE(callback_called_1);
}

TEST_F(ScopeExitGuardTest, reset) {
    io::ScopeExitGuard guard([&]() {
        ASSERT_FALSE(false); // fail test if callback was called
    });

    guard.reset();
}

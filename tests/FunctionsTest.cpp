/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "fs/Functions.h"

struct FunctionsTest : public testing::Test,
                       public LogRedirector {
    FunctionsTest() {
        m_tmp_test_dir = create_temp_test_directory();
    }

protected:
   io::fs::Path m_tmp_test_dir;
};

TEST_F(FunctionsTest, stat_file) {
    const auto path_str = create_empty_file(m_tmp_test_dir.string());
    ASSERT_FALSE(path_str.empty());

    std::size_t on_stat_call_count = 0;

    io::EventLoop loop;
    io::fs::stat(loop, path_str,
        [&](const io::fs::Path& path, const io::fs::StatData& data, const io::Error& error){
            EXPECT_FALSE(error) << error;
            EXPECT_EQ(path_str, path.string());
            EXPECT_EQ(0, data.size);
            EXPECT_TRUE(S_ISREG(data.mode)); // is regular file
            ++on_stat_call_count;
        }
    );

    EXPECT_EQ(0, on_stat_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_stat_call_count);
}

TEST_F(FunctionsTest, stat_not_existing) {
    std::size_t on_stat_call_count = 0;

    io::EventLoop loop;
    io::fs::stat(loop, m_tmp_test_dir / "/not_exists",
        [&](const io::fs::Path& path, const io::fs::StatData& data, const io::Error& error){
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::NO_SUCH_FILE_OR_DIRECTORY, error.code());
            ++on_stat_call_count;
        }
    );

    EXPECT_EQ(0, on_stat_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_stat_call_count);
}

TEST_F(FunctionsTest, stat_dir) {
    // TOOD: implement
}


#include "UTCommon.h"

#include "io/Error.h"

#include <uv.h>

struct ErrorTest : public testing::Test,
                   public LogRedirector {
};

TEST_F(ErrorTest, create_from_uv_error_code) {
    io::Error error(UV_EOF);
    EXPECT_EQ(io::StatusCode::END_OF_FILE, error.code());
    EXPECT_EQ("End of file", error.string());
}

TEST_F(ErrorTest, create_from_status_code) {
    io::Error error_1(io::StatusCode::END_OF_FILE); // uv error
    EXPECT_EQ(io::StatusCode::END_OF_FILE, error_1.code());
    EXPECT_EQ("End of file", error_1.string());

    io::Error error_2(io::StatusCode::UNDEFINED); // Custom error
    EXPECT_EQ(io::StatusCode::UNDEFINED, error_2.code());
    EXPECT_EQ("Unknown error", error_2.string());
}

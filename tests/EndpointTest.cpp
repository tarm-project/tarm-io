#include "UTCommon.h"

#include "io/Endpoint.h"

struct EndpointTest : public testing::Test,
                      public LogRedirector {
};

TEST_F(EndpointTest, default_constructor) {
    io::Endpoint endpoint;
    EXPECT_EQ(io::Endpoint::UNDEFINED, endpoint.type());
}
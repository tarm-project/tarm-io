#include "UTCommon.h"

#include "io/global/Configuration.h"

struct ConfigurationTest : public testing::Test,
                           public LogRedirector {
};

TEST_F(ConfigurationTest, receive_buffer_size) {
    std::cout << io::global::min_receive_buffer_size() << std::endl;
    std::cout << io::global::default_receive_buffer_size() << std::endl;
    std::cout << io::global::max_receive_buffer_size() << std::endl;

    std::cout << "===" << std::endl;

    std::cout << io::global::min_send_buffer_size() << std::endl;
    std::cout << io::global::default_send_buffer_size() << std::endl;
    std::cout << io::global::max_send_buffer_size() << std::endl;

}

#include <gtest/gtest.h>

#include "io/global/Configuration.h"
#include <iostream>

int main(int argc, char **argv) {
    io::global::set_logger_callback([](const std::string& message) {
        std::cout << message << std::endl;
    });

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
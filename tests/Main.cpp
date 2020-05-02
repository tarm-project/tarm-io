/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "io/global/Configuration.h"

#include <gtest/gtest.h>

#ifdef IO_BUILD_FOR_WINDOWS
    #include <openssl/applink.c>
#endif

#include <iostream>

int main(int argc, char **argv) {
    io::global::set_logger_callback([](const std::string& message) {
        std::cout << message << std::endl;
    });

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
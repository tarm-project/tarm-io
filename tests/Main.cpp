/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "io/global/Configuration.h"

#include <gtest/gtest.h>

#ifdef IO_BUILD_FOR_WINDOWS
    #include <openssl/applink.c>
#endif

#include <atomic>
#include <iostream>
#include <thread>

std::atomic<bool> stop_watchdog(false);

void hanged_tests_watchdog() {
    const auto LIMIT = std::chrono::minutes(5);

    auto seconds_counter = std::chrono::seconds(0);
    while(!stop_watchdog) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        seconds_counter += std::chrono::seconds(1);

        if (seconds_counter >= LIMIT) {
            break;
        }
    }

    if (stop_watchdog) {
        return;
    }

    const ::testing::TestInfo* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    if (test_info) {
        std::cout << "Total tests execution time is over limit of "  <<
            std::chrono::duration_cast<std::chrono::seconds>(LIMIT).count() <<
            " seconds, aborting. Last test: " << test_info->test_suite_name() << "." << test_info->name() << std::endl;
    }

    abort();
}

int main(int argc, char **argv) {
    io::global::set_logger_callback([](const std::string& message) {
        std::cout << message << std::endl;
    });

    std::thread watch_dog(hanged_tests_watchdog);

    ::testing::InitGoogleTest(&argc, argv);
    const auto result = RUN_ALL_TESTS();
    stop_watchdog = true;
    watch_dog.join();
    return result;
}

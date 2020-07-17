/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "global/Configuration.h"
#include "UTCommon.h"

#ifdef TARM_IO_PLATFORM_WINDOWS
    #include <openssl/applink.c>
#endif

#include <atomic>
#include <iostream>
#include <thread>

using namespace tarm;

std::atomic<bool> stop_watchdog(false);

void hanged_tests_watchdog() {
    const auto LIMIT = std::chrono::minutes(20);

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

    if (current_test_suite_name().size() && current_test_case_name().size()) {
        std::cout << "Total tests execution time is over limit of "  <<
            std::chrono::duration_cast<std::chrono::seconds>(LIMIT).count() <<
            " seconds, aborting. Last test: " << current_test_suite_name() << "." << current_test_case_name() << std::endl;
    }

    abort();
}

int main(int argc, char **argv) {
    // For XCode ann here non-paused breakpoint with the following command: process handle SIGHUP -s false
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

/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "io/detail/ConstexprString.h"

#include "io/Logger.h"

struct LoggerTest : public testing::Test,
                    public LogRedirector {

};

TEST_F(LoggerTest, log_with_prefix) {
    io::Logger logger("PREFIX");

    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[INFO] <PREFIX> Single message", message);
    });
    logger.log(io::Logger::Severity::INFO, "Single message");

    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[WARNING] <PREFIX> Multiple chunks", message);
    });
    logger.log(io::Logger::Severity::WARNING, "Multiple", "chunks");

    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[ERROR] <PREFIX> int: 11 double: 1.2", message);
    });
    logger.log(io::Logger::Severity::ERROR, "int:", 11, "double:", 1.2);
}

TEST_F(LoggerTest, log_withot_prefix) {
    io::Logger logger;

    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[INFO] Single message", message);
    });
    logger.log(io::Logger::Severity::INFO, "Single message");

    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[WARNING] Multiple chunks", message);
    });
    logger.log(io::Logger::Severity::WARNING, "Multiple", "chunks");

    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[ERROR] int: 11 double: 1.2", message);
    });
    logger.log(io::Logger::Severity::ERROR, "int:", 11, "double:", 1.2);
}

namespace {

struct S {
    S(int value_) :
    value(value_) {
    }

    int value = -1;
};

std::ostream& operator<< (std::ostream& os, const S& s) {
    return os << "S[" << s.value << "]";
}

} // namespace

TEST_F(LoggerTest, log_with_custom_ostrem_operator) {
    S s(100500);

    io::Logger logger;
    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[INFO] Hello, S[100500]", message);
    });
    logger.log(io::Logger::Severity::INFO, "Hello,", s);
}

TEST_F(LoggerTest, log_with_file_and_line) {
    io::Logger logger;

    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[INFO] [SomeFile.xxx:123] (foo) Single message", message);
    });
    logger.log_with_compile_context(io::Logger::Severity::INFO, "SomeFile.xxx", 123, "foo", "Single message");
}

TEST_F(LoggerTest, log_via_macro) {
    io::Logger logger;
    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[WARNING] [LoggerTest.cpp:90] (TestBody) Message 12345", message);
    });
    IO_LOG((&logger), WARNING, "Message", 12345);
}

TEST_F(LoggerTest, log_pointer) {
    std::unique_ptr<S> ptr(new S(42));

    io::Logger logger;
    logger.enable_log([](const std::string& message) {
        EXPECT_NE(std::string::npos, message.find("[ERROR] [LoggerTest.cpp:100] (TestBody) Message 0x"));
    });
    IO_LOG((&logger), ERROR, "Message", ptr.get());
}

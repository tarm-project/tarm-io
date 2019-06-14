#include "UTCommon.h"

#include "io/Logger.h"

struct LoggerTest : public testing::Test,
                    public LogRedirector {

};

TEST_F(LoggerTest, low_with_prefix) {
    io::Logger logger("PREFIX");

    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[INFO] <PREFIX> Single message", message);
    });
    logger.log(io::Logger::Severity::INFO, "Single message");

    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[WARNING] <PREFIX> Multiple chunks", message);
    });
    logger.log(io::Logger::Severity::WARNING, "Multiple", " chunks");

    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[ERROR] <PREFIX> int:11, double:1.2", message);
    });
    logger.log(io::Logger::Severity::ERROR, "int:", 11, ", double:", 1.2);
}

TEST_F(LoggerTest, low_withot_prefix) {
    io::Logger logger;

    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[INFO] Single message", message);
    });
    logger.log(io::Logger::Severity::INFO, "Single message");

    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[WARNING] Multiple chunks", message);
    });
    logger.log(io::Logger::Severity::WARNING, "Multiple", " chunks");

    logger.enable_log([](const std::string& message) {
        EXPECT_EQ("[ERROR] int:11, double:1.2", message);
    });
    logger.log(io::Logger::Severity::ERROR, "int:", 11, ", double:", 1.2);
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
    logger.log(io::Logger::Severity::INFO, "Hello, ", s);
}
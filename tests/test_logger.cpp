#include "mcp/common/macros.h"
#include "mcp/logger/logger.h"

#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>

namespace {

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 使用临时日志文件
        log_file_ = "/tmp/solarmcp_test_logger.log";
        // 确保干净状态
        std::remove(log_file_.c_str());
    }

    void TearDown() override {
        mcp::Logger::getInstance().shutdown();
        std::remove(log_file_.c_str());
    }

    std::string log_file_;
};

TEST_F(LoggerTest, InitAndShutdown) {
    auto& logger = mcp::Logger::getInstance();
    EXPECT_FALSE(logger.isInitialized());

    logger.init(log_file_, mcp::LogLevel::DEBUG);
    EXPECT_TRUE(logger.isInitialized());

    logger.shutdown();
    EXPECT_FALSE(logger.isInitialized());
}

TEST_F(LoggerTest, LogMessagesWrittenToFile) {
    auto& logger = mcp::Logger::getInstance();
    logger.init(log_file_, mcp::LogLevel::TRACE);

    LOG_INFO("Test message: {}", 42);
    LOG_DEBUG("Debug value: {}", "hello");

    // 给异步日志器时间刷新
    logger.shutdown();

    // 读取日志文件
    std::ifstream in(log_file_);
    ASSERT_TRUE(in.is_open());
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("INFO "), std::string::npos);
    EXPECT_NE(content.find("Test message: 42"), std::string::npos);
    EXPECT_NE(content.find("DEBUG"), std::string::npos);
}

TEST_F(LoggerTest, LogLevelFiltering) {
    auto& logger = mcp::Logger::getInstance();
    logger.init(log_file_, mcp::LogLevel::WARN);

    LOG_TRACE("Should be dropped");
    LOG_DEBUG("Should be dropped");
    LOG_INFO("Should be dropped");
    LOG_WARN("Warning: {}", 1);
    LOG_ERROR("Error: {}", 2);

    logger.shutdown();

    std::ifstream in(log_file_);
    ASSERT_TRUE(in.is_open());
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    EXPECT_EQ(content.find("Should be dropped"), std::string::npos);
    EXPECT_NE(content.find("Warning: 1"), std::string::npos);
    EXPECT_NE(content.find("Error: 2"), std::string::npos);
}

TEST_F(LoggerTest, LevelChangeAtRuntime) {
    auto& logger = mcp::Logger::getInstance();
    logger.init(log_file_, mcp::LogLevel::INFO);

    LOG_DEBUG("Debug before change");
    logger.setLevel(mcp::LogLevel::DEBUG);
    LOG_DEBUG("Debug after change");

    logger.shutdown();

    std::ifstream in(log_file_);
    ASSERT_TRUE(in.is_open());
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    EXPECT_EQ(content.find("Debug before change"), std::string::npos);
    EXPECT_NE(content.find("Debug after change"), std::string::npos);
}

} // anonymous namespace

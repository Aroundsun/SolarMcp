#include "mcp/logger/logger.h"
#include "mcp/common/macros.h"

#include <cstdio>
#include <iostream>

namespace mcp {

// ---------------------------------------------------------------------------
// Logger
// ---------------------------------------------------------------------------

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    if (async_logger_) {
        async_logger_->stop();
    }
}

void Logger::init(const std::string& log_file, LogLevel level) {
    level_ = level;
    async_logger_ = std::make_unique<AsyncLogger>();
    async_logger_->start(log_file);
    initialized_ = true;

    // 向 stderr 和日志文件输出启动横幅
    std::cerr << "[SolarMcp] Logger initialized: " << log_file << std::endl;
    LOG_INFO("Logger started — level={}, file={}",
             static_cast<int>(level), log_file);
}

void Logger::setLevel(LogLevel level) {
    level_ = level;
}

void Logger::setFile(const std::string& path) {
    if (async_logger_) {
        // 简单做法：停止旧后端，启动新后端
        async_logger_->stop();
        async_logger_->start(path);
    }
}

void Logger::shutdown() {
    if (async_logger_) {
        LOG_INFO("Logger shutting down");
        async_logger_->stop();
        initialized_ = false;
    }
}

} // namespace mcp

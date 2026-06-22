#pragma once

#include "mcp/logger/async_logger.h"
#include "mcp/logger/log_level.h"
#include "mcp/common/noncopyable.h"

#include <fmt/core.h>

#include <memory>
#include <string>

namespace mcp {

/// Logger 单例，提供统一的日志门面。
///
/// 日志消息通过 fmtlib 格式化，并发送到 AsyncLogger 后端，
/// 后者负责双缓冲异步磁盘 I/O。
///
/// 线程安全：所有公开方法均可从任意线程调用。
class Logger : public NonCopyable {
public:
    /// 获取全局 Logger 实例。
    static Logger& getInstance();

    /// 使用文件路径和级别初始化日志器。
    /// 必须在任何日志调用之前调用一次。
    /// @param log_file  日志文件路径（目录必须存在）
    /// @param level     记录的最低严重级别
    void init(const std::string& log_file, LogLevel level = LogLevel::INFO);

    /// 以给定级别记录格式化消息。
    /// 使用 fmtlib 风格格式字符串：log(INFO, "value={}", 42)
    template <typename... Args>
    void log(LogLevel level, fmt::format_string<Args...> fmt, Args&&... args);

    /// 设置最低日志级别（低于此级别的消息将被丢弃）。
    void setLevel(LogLevel level);

    /// 运行时更改输出日志文件路径。
    void setFile(const std::string& path);

    /// 刷新并关闭异步后端，阻塞直到完成。
    void shutdown();

    /// 日志器已初始化时返回 true。
    bool isInitialized() const noexcept { return initialized_; }

private:
    Logger() = default;
    ~Logger();

    std::unique_ptr<AsyncLogger> async_logger_;
    LogLevel level_ = LogLevel::INFO;
    bool initialized_ = false;
};

// 模板实现必须放在头文件中
template <typename... Args>
void Logger::log(LogLevel level, fmt::format_string<Args...> fmt, Args&&... args) {
    if (!initialized_ || level < level_) {
        return;
    }
    auto msg = fmt::format(fmt, std::forward<Args>(args)...);
    async_logger_->append(level, std::move(msg));
}

} // namespace mcp

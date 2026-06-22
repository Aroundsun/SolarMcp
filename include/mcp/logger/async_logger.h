#pragma once

#include "mcp/logger/log_level.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mcp {

/// 双缓冲异步日志后端。
///
/// 业务线程调用 append()，在短暂加锁下写入当前缓冲区。
/// 专用后端线程定期交换缓冲区并将满缓冲区刷新到磁盘。
///
/// 线程安全：append() 可从任意线程调用；start()/stop() 应由所有者调用。
class AsyncLogger {
public:
    /// 单条日志行的最大长度（截断前）。
    static constexpr size_t kMaxLineLength = 4096;

    /// 强制刷新前缓冲的日志行数。
    static constexpr size_t kFlushThreshold = 512;

    AsyncLogger();
    ~AsyncLogger();

    /// 启动后端刷新线程。
    /// @param file_path  日志文件路径
    void start(const std::string& file_path);

    /// 停止后端线程并刷新剩余数据。
    void stop();

    /// 将格式化日志条目追加到当前前端缓冲区。
    /// 热路径 — 由业务线程调用。
    void append(LogLevel level, std::string message);

private:
    /// 持有原始日志行的内部缓冲区。
    struct Buffer {
        std::vector<std::string> lines;
        size_t max_lines = 10000; // 强制交换前的最大行数
    };

    /// 后端刷新线程的主循环。
    void backendRoutine();

    /// 格式化单条日志：[timestamp] [LEVEL] message
    static std::string formatEntry(LogLevel level, const std::string& message);

    /// 将 LogLevel 转换为 5 字符标签。
    static const char* levelLabel(LogLevel level);

    // -- 双缓冲 --
    Buffer buffer_a_;
    Buffer buffer_b_;
    Buffer* current_buffer_;   // 指向当前写入缓冲区
    Buffer* flush_buffer_;     // 指向正在刷新的缓冲区

    // -- 同步 --
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> running_{false};

    // -- 后端 --
    std::thread backend_thread_;
    std::string file_path_;
    size_t flush_count_{0};
    static constexpr size_t kFlushIntervalMs = 100;
};

} // namespace mcp

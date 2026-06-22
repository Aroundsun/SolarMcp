#include "mcp/logger/async_logger.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace mcp {

// ---------------------------------------------------------------------------
// 构造 / 析构
// ---------------------------------------------------------------------------

AsyncLogger::AsyncLogger()
    : current_buffer_(&buffer_a_)
    , flush_buffer_(&buffer_b_)
{}

AsyncLogger::~AsyncLogger() {
    if (running_.load(std::memory_order_acquire)) {
        stop();
    }
}

// ---------------------------------------------------------------------------
// start / stop
// ---------------------------------------------------------------------------

void AsyncLogger::start(const std::string& file_path) {
    file_path_ = file_path;
    running_.store(true, std::memory_order_release);
    backend_thread_ = std::thread(&AsyncLogger::backendRoutine, this);
}

void AsyncLogger::stop() {
    running_.store(false, std::memory_order_release);
    cond_.notify_one();
    if (backend_thread_.joinable()) {
        backend_thread_.join();
    }

    // 最终刷新 — 持有 mutex 防止并发 append()
    // 在排空当前缓冲区时继续追加条目。
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!current_buffer_->lines.empty()) {
            std::ofstream out(file_path_, std::ios::app);
            if (out.is_open()) {
                for (const auto& line : current_buffer_->lines) {
                    out << line << '\n';
                }
            }
            current_buffer_->lines.clear();
        }
        // 同时排空可能已交换到 flush_buffer_ 的内容
        if (!flush_buffer_->lines.empty()) {
            std::ofstream out(file_path_, std::ios::app);
            if (out.is_open()) {
                for (const auto& line : flush_buffer_->lines) {
                    out << line << '\n';
                }
            }
            flush_buffer_->lines.clear();
        }
    }
}

// ---------------------------------------------------------------------------
// append — 热路径
// ---------------------------------------------------------------------------

void AsyncLogger::append(LogLevel level, std::string message) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_buffer_->lines.push_back(formatEntry(level, message));

        // 缓冲区将满时通知后端
        if (current_buffer_->lines.size() >= kFlushThreshold) {
            cond_.notify_one();
        }
    }
}

// ---------------------------------------------------------------------------
// backendRoutine — 在专用线程运行
// ---------------------------------------------------------------------------

void AsyncLogger::backendRoutine() {
    while (running_.load(std::memory_order_acquire)) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait_for(lock, std::chrono::milliseconds(kFlushIntervalMs),
                           [this] {
                               return !running_.load(std::memory_order_acquire)
                                   || current_buffer_->lines.size() >= kFlushThreshold;
                           });
        }

        // 交换缓冲区：当前变为刷新缓冲区
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (current_buffer_->lines.empty()) {
                continue;
            }
            std::swap(current_buffer_, flush_buffer_);
        }

        // 将 flush_buffer_ 写入磁盘（锁外）
        if (!flush_buffer_->lines.empty()) {
            std::ofstream out(file_path_, std::ios::app);
            if (out.is_open()) {
                for (const auto& line : flush_buffer_->lines) {
                    out << line << '\n';
                }
                out.flush();
            }
            flush_buffer_->lines.clear();
            ++flush_count_;
        }
    }
}

// ---------------------------------------------------------------------------
// formatEntry — 生成 [timestamp] [LEVEL] message
// ---------------------------------------------------------------------------

std::string AsyncLogger::formatEntry(LogLevel level,
                                      const std::string& message) {
    // 获取毫秒精度当前时间
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;

    std::tm tm_now{};
#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    // 格式：[2026-06-22 23:00:00.123] [INFO ] message
    char time_buf[64];
    std::snprintf(time_buf, sizeof(time_buf),
                  "%04d-%02d-%02d %02d:%02d:%02d.%03lld",
                  tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                  tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec,
                  static_cast<long long>(ms.count()));

    std::ostringstream oss;
    oss << '[' << time_buf << "] [" << levelLabel(level) << "] " << message;

    // 截断过长行
    std::string result = oss.str();
    if (result.size() > kMaxLineLength) {
        result.resize(kMaxLineLength - 4);
        result += "...";
    }
    return result;
}

const char* AsyncLogger::levelLabel(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
    }
    return "?????";
}

} // namespace mcp

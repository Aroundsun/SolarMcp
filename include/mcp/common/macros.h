#pragma once

// ---------------------------------------------------------------------------
// DISALLOW_COPY_MOVE — 禁用拷贝和移动，用于真正的单例
// ---------------------------------------------------------------------------
#define DISALLOW_COPY_MOVE(ClassName)            \
    ClassName(const ClassName&) = delete;        \
    ClassName& operator=(const ClassName&) = delete; \
    ClassName(ClassName&&) = delete;             \
    ClassName& operator=(ClassName&&) = delete

// ---------------------------------------------------------------------------
// DISALLOW_COPY — 禁用拷贝，允许移动
// ---------------------------------------------------------------------------
#define DISALLOW_COPY(ClassName)                 \
    ClassName(const ClassName&) = delete;        \
    ClassName& operator=(const ClassName&) = delete

// ---------------------------------------------------------------------------
// 日志宏 — 委托给 Logger 单例
// ---------------------------------------------------------------------------
// 用户需自行包含前向声明头文件
// 使用可变参数宏将格式化参数转发给 Logger

#define LOG_TRACE(fmt, ...) \
    ::mcp::Logger::getInstance().log(::mcp::LogLevel::TRACE, fmt, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) \
    ::mcp::Logger::getInstance().log(::mcp::LogLevel::DEBUG, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    ::mcp::Logger::getInstance().log(::mcp::LogLevel::INFO, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    ::mcp::Logger::getInstance().log(::mcp::LogLevel::WARN, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    ::mcp::Logger::getInstance().log(::mcp::LogLevel::ERROR, fmt, ##__VA_ARGS__)

#define LOG_FATAL(fmt, ...) \
    ::mcp::Logger::getInstance().log(::mcp::LogLevel::FATAL, fmt, ##__VA_ARGS__)

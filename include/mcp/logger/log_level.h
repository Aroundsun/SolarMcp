#pragma once

#include <cstdint>

namespace mcp {

/// 日志严重级别，按重要性递增排列。
enum class LogLevel : uint8_t {
    TRACE = 0,  ///< 最详细的日志，仅用于开发
    DEBUG = 1,  ///< 诊断信息
    INFO  = 2,  ///< 一般操作信息
    WARN  = 3,  ///< 潜在有害情况
    ERROR = 4,  ///< 错误事件，可能仍允许继续操作
    FATAL = 5   ///< 严重错误，导致过早终止
};

} // namespace mcp

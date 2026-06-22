#pragma once

#include <cstdint>

namespace mcp {

/// 日志严重级别，按重要性递增排列。
enum class LogLevel : uint8_t {
    TRACE = 0,  ///< Verbose tracing, development only
    DEBUG = 1,  ///< Diagnostic information
    INFO  = 2,  ///< General operational messages
    WARN  = 3,  ///< Potentially harmful situations
    ERROR = 4,  ///< Error events that may still allow continued operation
    FATAL = 5   ///< Severe errors causing premature termination
};

} // namespace mcp

#pragma once

#include "mcp/common/types.h"

#include <nlohmann/json.hpp>
#include <string>

namespace mcp {

/// 所有 MCP 工具的抽象基类。
///
/// 工具无状态 — 不持有可变的 per-request 状态。
/// 因此天然线程安全，可并发执行而无需额外同步。
///
/// 添加新工具：
/// 1. 继承 Tool
/// 2. 实现 name()、description()、inputSchema() 和 execute()
/// 3. 通过 ToolManager::registerTool() 注册
class Tool {
public:
    virtual ~Tool() = default;

    /// 唯一工具标识（如 "read_file"）。
    virtual std::string name() const = 0;

    /// 展示给 MCP 客户端的可读描述。
    virtual std::string description() const = 0;

    /// 描述期望参数的 JSON Schema。
    virtual nlohmann::json inputSchema() const = 0;

    /// 使用给定参数和上下文执行工具。
    /// @param params  工具特定参数（对照 inputSchema 校验）
    /// @param ctx     单次请求执行上下文
    /// @return 包含 content 或 error 的 Result
    virtual Result execute(const nlohmann::json& params, Context& ctx) = 0;
};

} // namespace mcp

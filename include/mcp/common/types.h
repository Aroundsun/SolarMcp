#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace mcp {

// ---------------------------------------------------------------------------
// ErrorInfo — 轻量级错误描述符
// ---------------------------------------------------------------------------
struct ErrorInfo {
    int code = 0;
    std::string message;
};

// ---------------------------------------------------------------------------
// Result — 统一返回类型，携带内容或错误
// ---------------------------------------------------------------------------
struct Result {
    nlohmann::json content;
    bool is_error = false;
    std::optional<ErrorInfo> error;

    /// 创建携带 payload 的成功 Result。
    static Result ok(nlohmann::json c) {
        Result r;
        r.content = std::move(c);
        r.is_error = false;
        return r;
    }

    /// 创建带错误码和可读消息的错误 Result。
    static Result err(int code, std::string message) {
        Result r;
        r.is_error = true;
        r.error = ErrorInfo{code, std::move(message)};
        return r;
    }

    /// 便捷方法：创建 content 为空数组的错误 Result。
    static Result errWithContent(int code, std::string message) {
        Result r = err(code, std::move(message));
        r.content = nlohmann::json::array();
        return r;
    }
};

// ---------------------------------------------------------------------------
// Context — 单次请求的执行上下文
// ---------------------------------------------------------------------------
struct Context {
    std::string request_id;       // JSON-RPC 请求 id（字符串或数字）
    int timeout_ms = 30000;       // 执行超时（毫秒）
    bool cancelled = false;       // 取消标志（P1）
};

// ---------------------------------------------------------------------------
// JSON-RPC 2.0 标准错误码
// ---------------------------------------------------------------------------
namespace ErrorCodes {
    constexpr int kParseError     = -32700;  // 收到无效 JSON
    constexpr int kInvalidRequest = -32600;  // 发送的 JSON 不是有效 Request
    constexpr int kMethodNotFound = -32601;  // 方法不存在
    constexpr int kInvalidParams  = -32602;  // 无效的方法参数
    constexpr int kInternalError  = -32603;  // JSON-RPC 内部错误

    // MCP 专用错误码（-32000 至 -32099 由 JSON-RPC 保留）
    constexpr int kToolNotFound   = -32000;  // 请求的工具未找到
    constexpr int kToolError      = -32001;  // 工具执行错误
    constexpr int kTimeout        = -32002;  // 执行超时
    constexpr int kAuthRequired   = -32003;  // 需要认证但未提供凭证
    constexpr int kAuthFailed     = -32004;  // 认证凭证无效
    constexpr int kServerBusy     = -32005;  // 线程池队列已满
} // namespace ErrorCodes

} // namespace mcp

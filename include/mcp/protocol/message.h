#pragma once

#include "mcp/protocol/request.h"
#include "mcp/protocol/response.h"

#include <variant>

namespace mcp {

/// 持有任意 JSON-RPC 2.0 消息类型的 variant。
///
/// Request：客户端 → 服务器（期望响应）
/// Response：服务器 → 客户端（请求的响应）
/// Notification：客户端 → 服务器（不期望响应；id 为 null 的 Request）
using Message = std::variant<Request, Response>;

/// 检查 Message 是否为 Request。
inline bool isRequest(const Message& msg) {
    return std::holds_alternative<Request>(msg);
}

/// 检查 Message 是否为 Response。
inline bool isResponse(const Message& msg) {
    return std::holds_alternative<Response>(msg);
}

} // namespace mcp

#pragma once

#include "mcp/common/types.h"

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>

namespace mcp {

/// JSON-RPC 2.0 响应。
struct Response {
    std::string jsonrpc = "2.0";
    nlohmann::json result;
    std::variant<std::string, int, std::nullptr_t> id;
    std::optional<ErrorInfo> error;

    /// 用给定 result 创建成功响应。
    static Response success(std::variant<std::string, int, std::nullptr_t> id,
                            nlohmann::json result);

    /// 用错误码和消息创建错误响应。
    static Response makeError(std::variant<std::string, int, std::nullptr_t> id,
                              int code, std::string message);

    /// 从 JSON 对象解析 Response。
    static Response fromJson(const nlohmann::json& j);

    /// 序列化为 JSON 对象。
    nlohmann::json toJson() const;

    /// 为错误响应时返回 true。
    bool isError() const noexcept { return error.has_value(); }

    /// 返回 id 的字符串形式。
    std::string idString() const;
};

} // namespace mcp

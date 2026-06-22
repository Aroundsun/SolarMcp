#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <variant>

namespace mcp {

/// JSON-RPC 2.0 请求。
struct Request {
    std::string jsonrpc = "2.0";
    std::string method;
    nlohmann::json params = nlohmann::json::object();
    std::variant<std::string, int, std::nullptr_t> id;

    /// 从 JSON 对象解析 Request。
    static Request fromJson(const nlohmann::json& j);

    /// 序列化为 JSON 对象。
    nlohmann::json toJson() const;

    /// 为 notification（无 id 字段）时返回 true。
    bool isNotification() const;

    /// 返回 id 的字符串形式用于 map 查找，notification 时为空。
    std::string idString() const;
};

} // namespace mcp

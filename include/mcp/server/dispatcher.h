#pragma once

#include "mcp/protocol/request.h"
#include "mcp/protocol/response.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace mcp {

/// 使用 unordered_map 实现 O(1) 方法查找的分发器。
///
/// 严格基于 map 分发 — 无 if-else 链。
/// 每个 MCP 方法注册一个接收 params 并返回 Response 的处理函数。
class Dispatcher {
public:
    using Handler = std::function<nlohmann::json(
        const nlohmann::json& params)>;

    Dispatcher() = default;

    /// 为给定方法名注册处理函数。
    /// 覆盖同名方法的已有处理函数。
    void registerMethod(const std::string& method, Handler handler);

    /// 将 Request 分发给已注册处理函数。
    /// 返回可发送的 Response（成功或错误）。
    Response dispatch(const Request& request);

    /// 给定方法已注册处理函数时返回 true。
    bool hasMethod(const std::string& method) const;

    /// 返回已注册方法数量。
    size_t methodCount() const noexcept { return handlers_.size(); }

private:
    std::unordered_map<std::string, Handler> handlers_;
};

} // namespace mcp

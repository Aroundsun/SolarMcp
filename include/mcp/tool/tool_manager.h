#pragma once

#include "mcp/tool/tool.h"
#include "mcp/common/types.h"

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mcp {

/// 所有可用 MCP 工具的注册表。
///
/// 注册/注销与查询/调用分别使用写锁/读锁，支持 ThreadPool 并发 callTool
/// 与 plugins/reload 并发安全。
class ToolManager {
public:
    struct ToolInfo {
        std::string name;
        std::string description;
        nlohmann::json input_schema;
    };

    ToolManager() = default;

    bool registerTool(std::unique_ptr<Tool> tool);
    Tool* getTool(const std::string& name) const;
    std::vector<ToolInfo> listTools() const;

    Result callTool(const std::string& name,
                    const nlohmann::json& params,
                    Context& ctx);

    size_t size() const;
    bool empty() const;
    void clear();
    bool unregisterTool(const std::string& name);

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<Tool>> tools_;
};

} // namespace mcp

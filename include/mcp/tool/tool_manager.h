#pragma once

#include "mcp/tool/tool.h"
#include "mcp/common/types.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mcp {

/// 所有可用 MCP 工具的注册表。
///
/// 通过 unique_ptr 持有 Tool 实例。工具在启动时注册
///（内置与插件加载），请求处理阶段注册表只读。
///
/// 线程安全：注册后只读，无需加锁。
class ToolManager {
public:
    /// listTools() 返回的轻量工具元数据。
    struct ToolInfo {
        std::string name;
        std::string description;
        nlohmann::json input_schema;
    };

    ToolManager() = default;

    /// 注册工具，接管所有权。
    /// 同名工具已注册时返回 false。
    bool registerTool(std::unique_ptr<Tool> tool);

    /// 按名称查找工具，未找到返回 nullptr。
    Tool* getTool(const std::string& name) const;

    /// 列出所有已注册工具的元数据。
    std::vector<ToolInfo> listTools() const;

    /// 按名称调用工具，传入 params 和 context。
    /// 返回 Result — 成功为 ok，工具未找到或失败为 err。
    Result callTool(const std::string& name,
                    const nlohmann::json& params,
                    Context& ctx);

    /// 已注册工具数量。
    size_t size() const noexcept { return tools_.size(); }

    /// 无已注册工具时返回 true。
    bool empty() const noexcept { return tools_.empty(); }

    /// 释放所有已注册工具。
    /// 须在 PluginManager::unloadAll() / dlclose 之前调用，
    /// 否则插件分配的 Tool 可能在 .so 卸载后被析构。
    void clear();

private:
    std::unordered_map<std::string, std::unique_ptr<Tool>> tools_;
};

} // namespace mcp

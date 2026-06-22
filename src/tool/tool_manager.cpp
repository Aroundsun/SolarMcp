#include "mcp/tool/tool_manager.h"
#include "mcp/common/types.h"

namespace mcp {

// ---------------------------------------------------------------------------
// registerTool
// ---------------------------------------------------------------------------

bool ToolManager::registerTool(std::unique_ptr<Tool> tool) {
    if (!tool) {
        return false;
    }
    const std::string& name = tool->name();
    if (tools_.find(name) != tools_.end()) {
        return false; // 已注册
    }
    tools_[name] = std::move(tool);
    return true;
}

// ---------------------------------------------------------------------------
// getTool
// ---------------------------------------------------------------------------

Tool* ToolManager::getTool(const std::string& name) const {
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        return it->second.get();
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// listTools
// ---------------------------------------------------------------------------

std::vector<ToolManager::ToolInfo> ToolManager::listTools() const {
    std::vector<ToolInfo> result;
    result.reserve(tools_.size());
    for (const auto& kv : tools_) {
        ToolInfo info;
        info.name = kv.second->name();
        info.description = kv.second->description();
        info.input_schema = kv.second->inputSchema();
        result.push_back(std::move(info));
    }
    return result;
}

// ---------------------------------------------------------------------------
// callTool
// ---------------------------------------------------------------------------

Result ToolManager::callTool(const std::string& name,
                              const nlohmann::json& params,
                              Context& ctx) {
    Tool* tool = getTool(name);
    if (!tool) {
        return Result::err(ErrorCodes::kToolNotFound,
                           "Tool not found: " + name);
    }

    try {
        return tool->execute(params, ctx);
    } catch (const std::exception& e) {
        return Result::err(ErrorCodes::kToolError,
                           std::string("Tool execution error: ") + e.what());
    }
}

// ---------------------------------------------------------------------------
// clear
// ---------------------------------------------------------------------------

void ToolManager::clear() {
    tools_.clear();
}

} // namespace mcp

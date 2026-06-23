#include "mcp/tool/tool_manager.h"
#include "mcp/common/types.h"

#include <mutex>

namespace mcp {

bool ToolManager::registerTool(std::unique_ptr<Tool> tool) {
    if (!tool) {
        return false;
    }
    const std::string& name = tool->name();

    std::unique_lock lock(mutex_);
    if (tools_.find(name) != tools_.end()) {
        return false;
    }
    tools_[name] = std::move(tool);
    return true;
}

Tool* ToolManager::getTool(const std::string& name) const {
    std::shared_lock lock(mutex_);
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<ToolManager::ToolInfo> ToolManager::listTools() const {
    std::shared_lock lock(mutex_);
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

Result ToolManager::callTool(const std::string& name,
                              const nlohmann::json& params,
                              Context& ctx) {
    Tool* tool = nullptr;
    {
        std::shared_lock lock(mutex_);
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            tool = it->second.get();
        }
    }

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

void ToolManager::clear() {
    std::unique_lock lock(mutex_);
    tools_.clear();
}

bool ToolManager::unregisterTool(const std::string& name) {
    std::unique_lock lock(mutex_);
    return tools_.erase(name) > 0;
}

size_t ToolManager::size() const {
    std::shared_lock lock(mutex_);
    return tools_.size();
}

bool ToolManager::empty() const {
    std::shared_lock lock(mutex_);
    return tools_.empty();
}

} // namespace mcp

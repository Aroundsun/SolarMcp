#pragma once

#include "mcp/plugin/plugin_abi.h"
#include "mcp/tool/tool.h"

#include <string>

namespace mcp {

/// 将插件 C execute 回调包装为内置 Tool（仅主程序使用）。
class CallbackTool : public Tool {
public:
    CallbackTool(std::string name,
                   std::string description,
                   std::string input_schema_json,
                   mcp_tool_execute_fn execute_fn);

    std::string name() const override { return name_; }
    std::string description() const override { return description_; }
    nlohmann::json inputSchema() const override;

    Result execute(const nlohmann::json& params, Context& ctx) override;

private:
    std::string name_;
    std::string description_;
    std::string input_schema_json_;
    mcp_tool_execute_fn execute_fn_;
};

} // namespace mcp

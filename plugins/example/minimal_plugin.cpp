/// 最小插件示例：演示 C ABI 注册流程，不含业务逻辑。
///
/// 可复制本文件作为新插件的起点。工具名 plugin_demo，与内置 read_file 无冲突。

#include "mcp/tool/tool.h"
#include "mcp/tool/tool_manager.h"

#include <memory>
#include <string>

namespace {

class DemoTool : public mcp::Tool {
public:
    std::string name() const override { return "plugin_demo"; }

    std::string description() const override {
        return "Minimal plugin demo tool";
    }

    nlohmann::json inputSchema() const override {
        return {
            {"type", "object"},
            {"properties", nlohmann::json::object()}
        };
    }

    mcp::Result execute(const nlohmann::json& /*params*/,
                        mcp::Context& /*ctx*/) override {
        return mcp::Result::ok({{"message", "hello from minimal plugin"}});
    }
};

} // namespace

extern "C" {

const char* mcp_plugin_name() {
    return "minimal_plugin";
}

const char* mcp_plugin_version() {
    return "1.0.0";
}

int mcp_plugin_register(mcp::ToolManager* manager) {
    if (!manager) {
        return -1;
    }

    auto tool = std::make_unique<DemoTool>();
    if (!manager->registerTool(std::move(tool))) {
        return -2;
    }

    return 0;
}

} // extern "C"

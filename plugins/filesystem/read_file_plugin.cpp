/// 演示插件：通过 C ABI 注册 ReadFile 工具。
///
/// 演示外部 .so 如何在运行时向 SolarMcp 服务器
/// 贡献工具而无需重新编译。

#include "mcp/tool/tool_manager.h"
#include "mcp/tool/read_file_tool.h"

#include <memory>

extern "C" {

const char* mcp_plugin_name() {
    return "read_file_plugin";
}

const char* mcp_plugin_version() {
    return "1.0.0";
}

int mcp_plugin_register(mcp::ToolManager* manager) {
    if (!manager) {
        return -1; // 错误：manager 为空
    }

    auto tool = std::make_unique<mcp::ReadFileTool>(
        10 * 1024 * 1024,                         // 最大 10 MB
        std::vector<std::string>{"/tmp", "/home"} // 允许的路径
    );

    if (!manager->registerTool(std::move(tool))) {
        return -2; // 错误：工具名重复
    }

    return 0; // 成功
}

} // extern "C"

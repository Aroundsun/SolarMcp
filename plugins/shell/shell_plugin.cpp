/// Shell Tool 插件：通过 C ABI 注册 Shell 命令执行工具。
///
/// 可选导出 mcp_plugin_set_config_path()，从 config.yaml 读取 tools.shell 配置。

#include "plugins/shell/shell_tool.h"
#include "mcp/tool/tool_manager.h"

#include <yaml-cpp/yaml.h>

#include <memory>
#include <string>
#include <vector>

namespace {

std::string g_config_path;

struct ShellPluginConfig {
    bool enabled = true;
    int timeout_sec = 30;
    size_t max_output_bytes = 10 * 1024 * 1024;
    std::vector<std::string> allowed_shells{"/bin/sh", "/bin/bash"};
};

ShellPluginConfig loadShellConfig() {
    ShellPluginConfig cfg;
    if (g_config_path.empty()) {
        return cfg;
    }

    try {
        YAML::Node root = YAML::LoadFile(g_config_path);
        YAML::Node shell = root["tools"]["shell"];
        if (!shell || !shell.IsMap()) {
            return cfg;
        }

        if (shell["enabled"]) {
            cfg.enabled = shell["enabled"].as<bool>();
        }
        if (shell["timeout_sec"]) {
            cfg.timeout_sec = shell["timeout_sec"].as<int>();
        }
        if (shell["max_output_mb"]) {
            cfg.max_output_bytes =
                static_cast<size_t>(shell["max_output_mb"].as<int>()) * 1024 * 1024;
        }
        if (shell["allowed_shells"] && shell["allowed_shells"].IsSequence()) {
            cfg.allowed_shells.clear();
            for (const auto& item : shell["allowed_shells"]) {
                cfg.allowed_shells.push_back(item.as<std::string>());
            }
        }
    } catch (...) {
        // 配置解析失败时使用默认值
    }

    return cfg;
}

} // namespace

extern "C" {

const char* mcp_plugin_name() {
    return "shell_plugin";
}

const char* mcp_plugin_version() {
    return "1.0.0";
}

void mcp_plugin_set_config_path(const char* path) {
    g_config_path = path ? path : "";
}

int mcp_plugin_register(mcp::ToolManager* manager) {
    if (!manager) {
        return -1;
    }

    ShellPluginConfig cfg = loadShellConfig();
    if (!cfg.enabled) {
        return 0;
    }

    auto tool = std::make_unique<mcp::ShellTool>(
        cfg.timeout_sec,
        std::move(cfg.allowed_shells),
        cfg.max_output_bytes
    );

    if (!manager->registerTool(std::move(tool))) {
        return -2;
    }

    return 0;
}

} // extern "C"

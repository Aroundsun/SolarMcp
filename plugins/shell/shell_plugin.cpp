/// Shell 插件：纯 C ABI 注册，shell 逻辑仍在同 .so 内的 ShellTool（不跨边界传 C++ 对象）。

#include "plugins/shell/shell_tool.h"
#include "mcp/plugin/plugin_abi.h"

#include <yaml-cpp/yaml.h>

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

const mcp_host_api* g_host = nullptr;
mcp::ShellTool* g_shell_tool = nullptr;

struct ShellPluginConfig {
    bool enabled = true;
    int timeout_sec = 30;
    size_t max_output_bytes = 10 * 1024 * 1024;
    std::vector<std::string> allowed_shells{"/bin/sh", "/bin/bash"};
};

ShellPluginConfig loadShellConfig(const char* config_path) {
    ShellPluginConfig cfg;
    if (!config_path || !*config_path) {
        return cfg;
    }

    try {
        YAML::Node root = YAML::LoadFile(config_path);
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
        // 使用默认值
    }

    return cfg;
}

int shellExecute(const char* params_json, char** out_json, char** err_msg) {
    if (!g_shell_tool) {
        if (err_msg) {
            *err_msg = ::strdup("shell tool not initialized");
        }
        return MCP_PLUGIN_ERR_INTERNAL;
    }

    try {
        auto params = nlohmann::json::parse(params_json ? params_json : "{}");
        mcp::Context ctx;
        auto result = g_shell_tool->execute(params, ctx);

        if (result.is_error) {
            if (err_msg) {
                *err_msg = ::strdup(result.error->message.c_str());
            }
            return MCP_PLUGIN_ERR_INTERNAL;
        }

        std::string body = result.content.dump();
        *out_json = static_cast<char*>(std::malloc(body.size() + 1));
        if (!*out_json) {
            return MCP_PLUGIN_ERR_INTERNAL;
        }
        std::memcpy(*out_json, body.c_str(), body.size() + 1);
        return MCP_PLUGIN_OK;
    } catch (const std::exception& e) {
        if (err_msg) {
            *err_msg = ::strdup(e.what());
        }
        return MCP_PLUGIN_ERR_INTERNAL;
    }
}

} // namespace

extern "C" {

int mcp_plugin_abi_version(void) {
    return SOLARMCP_PLUGIN_ABI;
}

const char* mcp_plugin_version(void) {
    return "2.0.0";
}

const char* mcp_plugin_name(void) {
    return "shell_plugin";
}

int mcp_plugin_init(const mcp_host_api* host) {
    if (!host || host->abi_version != SOLARMCP_PLUGIN_ABI ||
        !host->register_tool) {
        return MCP_PLUGIN_ERR_HOST;
    }
    g_host = host;
    return MCP_PLUGIN_OK;
}

int mcp_plugin_register(void) {
    if (!g_host) {
        return MCP_PLUGIN_ERR_HOST;
    }

    const char* config_path = g_host->get_config_path
        ? g_host->get_config_path(g_host->host_ctx)
        : "";

    ShellPluginConfig cfg = loadShellConfig(config_path);
    if (!cfg.enabled) {
        delete g_shell_tool;
        g_shell_tool = nullptr;
        return MCP_PLUGIN_OK;
    }

    delete g_shell_tool;
    g_shell_tool = new mcp::ShellTool(
        cfg.timeout_sec,
        std::move(cfg.allowed_shells),
        cfg.max_output_bytes);

    char schema_buf[512];
    std::snprintf(schema_buf, sizeof(schema_buf),
        "{\"type\":\"object\",\"properties\":{"
        "\"command\":{\"type\":\"string\",\"description\":\"Shell command to execute\"},"
        "\"timeout_seconds\":{\"type\":\"integer\",\"description\":\"Timeout in seconds (default: %d)\"}"
        "},\"required\":[\"command\"]}",
        cfg.timeout_sec);

    return g_host->register_tool(
        g_host->host_ctx,
        "shell",
        "Execute a shell command and return stdout, stderr and exit code",
        schema_buf,
        shellExecute);
}

void mcp_plugin_shutdown(void) {
    delete g_shell_tool;
    g_shell_tool = nullptr;
    g_host = nullptr;
}

} // extern "C"

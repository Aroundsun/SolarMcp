/**
 * 最小插件示例（纯 C）。
 * 演示 plugin_abi.h 用法，不含业务逻辑。
 */

#include "mcp/plugin/plugin_abi.h"

#include <stdlib.h>
#include <string.h>

static const mcp_host_api* g_host;

static int demo_execute(const char* params_json,
                        char** out_json,
                        char** err_msg) {
    (void)params_json;
    (void)err_msg;

    static const char kBody[] = "{\"message\":\"hello from minimal plugin\"}";
    *out_json = strdup(kBody);
    if (!*out_json) {
        return MCP_PLUGIN_ERR_INTERNAL;
    }
    return MCP_PLUGIN_OK;
}

int mcp_plugin_abi_version(void) {
    return SOLARMCP_PLUGIN_ABI;
}

const char* mcp_plugin_version(void) {
    return "2.0.0";
}

const char* mcp_plugin_name(void) {
    return "minimal_plugin";
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

    static const char kSchema[] =
        "{\"type\":\"object\",\"properties\":{}}";

    return g_host->register_tool(
        g_host->host_ctx,
        "plugin_demo",
        "Minimal plugin demo tool (pure C ABI)",
        kSchema,
        demo_execute);
}

void mcp_plugin_shutdown(void) {
    g_host = NULL;
}

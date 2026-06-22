#pragma once

/**
 * SolarMcp 插件 ABI（纯 C）。
 *
 * 插件与主程序之间的唯一二进制契约。不兼容变更须将 SOLARMCP_PLUGIN_ABI +1。
 * 插件不得包含主程序 C++ 头文件（如 tool_manager.h）。
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOLARMCP_PLUGIN_ABI 1

/** 插件 register/init 返回码 */
#define MCP_PLUGIN_OK           0
#define MCP_PLUGIN_ERR_HOST    -1  /* host 无效或 init 失败 */
#define MCP_PLUGIN_ERR_DUP     -2  /* 工具名重复 */
#define MCP_PLUGIN_ERR_INTERNAL -3 /* 插件内部错误 */

/** 宿主日志级别（与插件共享） */
#define MCP_LOG_TRACE 0
#define MCP_LOG_DEBUG 1
#define MCP_LOG_INFO  2
#define MCP_LOG_WARN  3
#define MCP_LOG_ERROR 4

/**
 * 工具 execute 回调。
 *
 * @param params_json  输入参数 JSON 字符串
 * @param out_json     成功时输出 JSON（插件用 malloc 分配，宿主 free）
 * @param err_msg      失败时错误信息（插件用 malloc 分配，宿主 free）
 * @return MCP_PLUGIN_OK 或负值错误码
 */
typedef int (*mcp_tool_execute_fn)(const char* params_json,
                                   char** out_json,
                                   char** err_msg);

/**
 * 宿主提供给插件的能力表。
 * 仅追加字段，不可重排；变更须 SOLARMCP_PLUGIN_ABI +1。
 */
typedef struct mcp_host_api {
    int abi_version;
    void* host_ctx;

    /**
     * 注册工具。
     * @return MCP_PLUGIN_OK 或 MCP_PLUGIN_ERR_DUP
     */
    int (*register_tool)(void* host_ctx,
                         const char* name,
                         const char* description,
                         const char* input_schema_json,
                         mcp_tool_execute_fn execute);

    void (*log)(void* host_ctx, int level, const char* message);

    /** 返回 server config.yaml 路径，无则返回 "" */
    const char* (*get_config_path)(void* host_ctx);
} mcp_host_api;

/* ---- 插件必须导出 ---- */

/** 必须等于 SOLARMCP_PLUGIN_ABI */
int mcp_plugin_abi_version(void);

/** 插件语义版本，如 "1.0.0" */
const char* mcp_plugin_version(void);

/** 插件逻辑名，如 "shell_plugin" */
const char* mcp_plugin_name(void);

/** 接收宿主能力表，保存供 register 使用 */
int mcp_plugin_init(const mcp_host_api* host);

/** 向宿主注册工具 */
int mcp_plugin_register(void);

/** 可选：卸载前清理 */
void mcp_plugin_shutdown(void);

#ifdef __cplusplus
}
#endif

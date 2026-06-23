# 插件 ABI（纯 C）

SolarMcp 插件与主程序之间通过 **纯 C ABI** 通信。插件不得 `#include` 主程序的 C++ 头文件（如 `tool_manager.h`），也不得跨 `.so` 边界传递 C++ 对象。

唯一契约头文件：`include/mcp/plugin/plugin_abi.h`。

## 设计目标

| 问题（旧模式） | 新方案 |
|----------------|--------|
| 插件直接链接主程序 C++ 类，编译器/标准库版本必须一致 | 插件只依赖 C 函数指针表 |
| 跨 `.so` 传递 `ToolManager*`、`Tool` 析构顺序易崩溃 | 宿主用 `CallbackTool` 包装 C 回调 |
| 无 ABI 版本校验 | `mcp_plugin_abi_version()` 不匹配则拒绝加载 |
| 热重载可能加载坏插件后卸载旧版 | 先 `validatePlugin` 试加载，成功后再替换 |

## 加载流程

```
dlopen(.so)
  → dlsym 解析导出符号
  → mcp_plugin_abi_version() == SOLARMCP_PLUGIN_ABI ?
  → mcp_plugin_init(&host_api)
  → mcp_plugin_register()
  → 失败则 mcp_plugin_shutdown() + dlclose
```

宿主通过 `mcp_host_api` 提供：

- `register_tool` — 注册 MCP 工具（execute 为 C 回调）
- `log` — 写服务器日志
- `get_plugin_config_path` — 返回**本插件**配置文件路径（Core 发现，插件自行解析）

## 插件必须导出的符号

| 符号 | 说明 |
|------|------|
| `mcp_plugin_abi_version` | 返回 `SOLARMCP_PLUGIN_ABI`（当前为 **2**） |
| `mcp_plugin_version` | 语义版本字符串，如 `"2.0.0"` |
| `mcp_plugin_name` | 逻辑名，如 `"shell_plugin"` |
| `mcp_plugin_init` | 接收 `mcp_host_api*`，保存供 register 使用 |
| `mcp_plugin_register` | 向宿主注册工具 |
| `mcp_plugin_shutdown` | 可选，卸载前释放插件内资源 |

## execute 回调约定

```c
typedef int (*mcp_tool_execute_fn)(const char* params_json,
                                   char** out_json,
                                   char** err_msg);
```

- 成功：`MCP_PLUGIN_OK`，`*out_json` 为 JSON 字符串（`malloc` 分配，宿主 `free`）
- 失败：负值错误码，`*err_msg` 为错误信息（`malloc` 分配，宿主 `free`）

## 编写新插件

1. 只 `#include "mcp/plugin/plugin_abi.h"`
2. 参考 `plugins/example/minimal_plugin.c`（纯 C 最小示例）
3. 复杂逻辑可在同 `.so` 内用 C++ 实现，通过 C 回调桥接（见 `plugins/shell/shell_plugin.cpp`）
4. 在 `plugins/CMakeLists.txt` 添加 `add_library(... MODULE ...)`
5. 构建后 `.so` 与 `*.yaml` 置于 `plugins/{name}/` 子目录

配置管理见 [配置管理.md](./配置管理.md)。

**不要**：

- 链接 `libsolarmcp.a` 或包含 `tool_manager.h`
- 依赖主程序 `-rdynamic` 导出符号
- 在 ABI 结构体中重排或删除已有字段（须 `SOLARMCP_PLUGIN_ABI + 1`）

## ABI 版本升级

不兼容变更（如修改 `mcp_host_api` 字段顺序、变更 execute 签名）时：

1. 将 `plugin_abi.h` 中 `SOLARMCP_PLUGIN_ABI` 加 1
2. 主程序拒绝加载旧 ABI 插件并打 WARN 日志
3. 所有插件更新 `mcp_plugin_abi_version()` 返回值并重新编译

## 热重载与安全

- **单插件重载**：`reloadPlugin` 先 `validatePlugin`（试加载 + shutdown），通过后卸载旧版再正式加载
- **目录重载**：`reloadFromDirectory` 对所有 `.so` 逐个 validate，**全部通过**才 `unloadAll` 并重新加载
- validate 失败时**保留**当前已加载插件，不会半更新

详见 [插件热重载.md](./插件热重载.md)。架构背景见 [系统设计 §1.3.10](./系统设计.md#1310-插件系统)。安全见 [插件安全.md](./插件安全.md)。

## 相关文件

| 文件 | 说明 |
|------|------|
| `include/mcp/plugin/plugin_abi.h` | C ABI 契约 |
| `include/mcp/plugin/callback_tool.h` | 宿主侧 C 回调 → Tool 适配 |
| `src/plugin/callback_tool.cpp` | CallbackTool 实现 |
| `src/plugin/plugin_manager.cpp` | 加载、校验、热重载 |
| `plugins/example/minimal_plugin.c` | 纯 C 模板 |
| `plugins/shell/shell_plugin.cpp` | C++ 业务 + C ABI 桥接示例 |

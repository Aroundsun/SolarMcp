#pragma once

#include <string>
#include <vector>

namespace mcp {

class ToolManager;
class Tool;

/// 使用 POSIX dlopen/dlsym/dlclose 的动态插件加载器。
///
/// 扫描目录中的 .so 文件，逐个加载并调用
/// C 导出注册函数向 ToolManager 添加工具。
///
/// 插件 ABI 约定：
///   extern "C" const char* mcp_plugin_name();
///   extern "C" const char* mcp_plugin_version();
///   extern "C" int mcp_plugin_register(mcp::ToolManager* manager);
///
/// 可选（加载前调用）：
///   extern "C" void mcp_plugin_set_config_path(const char* path);
class PluginManager {
public:
    PluginManager() = default;
    ~PluginManager();

    /// 扫描 `plugin_dir` 中的 .so 并加载各插件。
    /// 各插件注册函数以给定 ToolManager 调用。
    /// @return 成功加载的插件数量
    int loadFromDirectory(const std::string& plugin_dir,
                          ToolManager& tool_manager,
                          const std::string& config_path = "");

    /// 卸载所有已加载插件并 dlclose 其句柄。
    ///
    /// 调用前须先 ToolManager::clear()，确保插件 Tool 在 .so 仍加载时析构。
    void unloadAll();

    /// 返回当前已加载插件数量。
    size_t loadedCount() const noexcept { return handles_.size(); }

private:
    /// 加载单个插件 .so 文件。
    /// @return 成功返回 true
    bool loadPlugin(const std::string& so_path, ToolManager& tool_manager,
                    const std::string& config_path);

    /// 所有已加载插件的 dlopen 句柄。
    std::vector<void*> handles_;
};

} // namespace mcp

#pragma once

#include <string>
#include <vector>

namespace mcp {

class ToolManager;
class Tool;

/// 插件热重载结果。
struct PluginReloadResult {
    int unloaded = 0;  ///< 卸载的插件数量
    int loaded = 0;    ///< 成功加载的插件数量
    int failed = 0;    ///< 加载失败的插件数量
};

/// 使用 POSIX dlopen/dlsym/dlclose 的动态插件加载器。
///
/// 扫描目录中的 .so 文件，逐个加载并调用
/// C 导出注册函数向 ToolManager 添加工具。
/// 支持运行时 reload（先卸载插件工具再重新 dlopen）。
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
    /// @return 成功加载的插件数量
    int loadFromDirectory(const std::string& plugin_dir,
                          ToolManager& tool_manager,
                          const std::string& config_path = "");

    /// 热重载：卸载当前所有插件工具并重新扫描目录加载。
    PluginReloadResult reloadFromDirectory(const std::string& plugin_dir,
                                           ToolManager& tool_manager,
                                           const std::string& config_path = "");

    /// 热重载单个插件 .so（按路径匹配）。
    /// @return 成功返回 true
    bool reloadPlugin(const std::string& so_path,
                      ToolManager& tool_manager,
                      const std::string& config_path = "");

    /// 卸载所有插件：先注销插件注册的工具，再 dlclose。
    ///
    /// 内置工具（非插件注册）不受影响。
    void unloadAll(ToolManager& tool_manager);

    /// 返回当前已加载插件数量。
    size_t loadedCount() const noexcept { return plugins_.size(); }

private:
    struct LoadedPlugin {
        void* handle = nullptr;
        std::string path;
        std::string name;
        std::vector<std::string> tool_names;
    };

    bool loadPlugin(const std::string& so_path, ToolManager& tool_manager,
                    const std::string& config_path);

    void unloadPlugin(LoadedPlugin& plugin, ToolManager& tool_manager);
    void unloadHandlesOnly();

    std::vector<LoadedPlugin> plugins_;
};

} // namespace mcp

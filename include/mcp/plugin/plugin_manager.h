#pragma once

#include <shared_mutex>
#include <string>
#include <vector>

namespace mcp {

class ToolManager;

/// 插件热重载结果。
struct PluginReloadResult {
    int unloaded = 0;
    int loaded = 0;
    int failed = 0;
};

/**
 * 动态插件加载器（纯 C ABI v2）。
 *
 * 扫描 plugins.directory 下各子目录中的 .so，发现并传递同目录插件配置路径。
 * Core 不解析插件业务配置。
 */
class PluginManager {
public:
    PluginManager() = default;
    ~PluginManager();

    int loadFromDirectory(const std::string& plugin_dir,
                          ToolManager& tool_manager);

    PluginReloadResult reloadFromDirectory(const std::string& plugin_dir,
                                           ToolManager& tool_manager);

    bool reloadPlugin(const std::string& so_path,
                      ToolManager& tool_manager);

    void unloadAll(ToolManager& tool_manager);

    size_t loadedCount() const;

private:
    struct LoadedPlugin {
        void* handle = nullptr;
        std::string path;
        std::string name;
        std::string version;
        int abi_version = 0;
        std::vector<std::string> tool_names;
    };

    bool loadPlugin(const std::string& so_path,
                    ToolManager& tool_manager,
                    const std::string& plugin_config_path);

    bool validatePlugin(const std::string& so_path,
                        ToolManager& trial_tool_manager,
                        const std::string& plugin_config_path);

    void unloadPlugin(LoadedPlugin& plugin, ToolManager& tool_manager);
    void unloadAllUnlocked(ToolManager& tool_manager);
    void unloadHandlesOnly();

    mutable std::shared_mutex mutex_;
    std::vector<LoadedPlugin> plugins_;
};

} // namespace mcp

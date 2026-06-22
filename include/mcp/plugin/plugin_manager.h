#pragma once

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
 * 动态插件加载器（纯 C ABI v1）。
 *
 * 插件须实现 plugin_abi.h 中导出符号，不得依赖主程序 C++ 类。
 * 加载时校验 SOLARMCP_PLUGIN_ABI，记录 mcp_plugin_version()。
 */
class PluginManager {
public:
    PluginManager() = default;
    ~PluginManager();

    int loadFromDirectory(const std::string& plugin_dir,
                          ToolManager& tool_manager,
                          const std::string& config_path = "");

    PluginReloadResult reloadFromDirectory(const std::string& plugin_dir,
                                           ToolManager& tool_manager,
                                           const std::string& config_path = "");

    bool reloadPlugin(const std::string& so_path,
                      ToolManager& tool_manager,
                      const std::string& config_path = "");

    void unloadAll(ToolManager& tool_manager);

    size_t loadedCount() const noexcept { return plugins_.size(); }

private:
    struct LoadedPlugin {
        void* handle = nullptr;
        std::string path;
        std::string name;
        std::string version;
        int abi_version = 0;
        std::vector<std::string> tool_names;
    };

    bool loadPlugin(const std::string& so_path, ToolManager& tool_manager,
                    const std::string& config_path);

    bool validatePlugin(const std::string& so_path,
                        ToolManager& trial_tool_manager,
                        const std::string& config_path);

    void unloadPlugin(LoadedPlugin& plugin, ToolManager& tool_manager);
    void unloadHandlesOnly();

    std::vector<LoadedPlugin> plugins_;
};

} // namespace mcp

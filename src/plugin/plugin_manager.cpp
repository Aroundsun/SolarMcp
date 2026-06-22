#include "mcp/plugin/plugin_manager.h"
#include "mcp/common/macros.h"
#include "mcp/tool/tool_manager.h"
#include "mcp/logger/logger.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <string>
#include <unordered_set>

namespace mcp {

namespace {

const char* dlErrorString() {
    const char* err = ::dlerror();
    return (err && *err) ? err : "unknown error";
}

std::unordered_set<std::string> toolNameSet(const ToolManager& tool_manager) {
    std::unordered_set<std::string> names;
    for (const auto& info : tool_manager.listTools()) {
        names.insert(info.name);
    }
    return names;
}

std::string pluginName(void* handle, const std::string& fallback) {
    using NameFunc = const char* (*)();
    ::dlerror();
    auto* name_fn = reinterpret_cast<NameFunc>(
        ::dlsym(handle, "mcp_plugin_name"));
    if (!name_fn) {
        return fallback;
    }
    const char* name = name_fn();
    return (name && *name) ? name : fallback;
}

void logRegisterFailure(const std::string& so_path, int code) {
    switch (code) {
    case -1:
        LOG_ERROR("Plugin {}: mcp_plugin_register failed — null ToolManager",
                  so_path);
        break;
    case -2:
        LOG_WARN("Plugin {}: mcp_plugin_register failed — tool name already "
                 "registered (duplicate)",
                 so_path);
        break;
    default:
        LOG_WARN("Plugin {}: mcp_plugin_register returned error code {}",
                 so_path, code);
        break;
    }
}

bool isPluginSoFile(const std::string& name) {
    if (name.size() < 4) return false;
    if (name.compare(name.size() - 3, 3, ".so") != 0) return false;
    if (name.size() > 4 && name[name.size() - 4] == '.') return false;
    return true;
}

} // namespace

PluginManager::~PluginManager() {
    if (!plugins_.empty()) {
        LOG_WARN("PluginManager destroyed with {} plugin(s) still loaded — "
                 "call unloadAll(tool_manager) first",
                 plugins_.size());
        unloadHandlesOnly();
    }
}

int PluginManager::loadFromDirectory(const std::string& plugin_dir,
                                      ToolManager& tool_manager,
                                      const std::string& config_path) {
    DIR* dir = ::opendir(plugin_dir.c_str());
    if (!dir) {
        LOG_WARN("Cannot open plugin directory '{}': {}",
                 plugin_dir, std::strerror(errno));
        return 0;
    }

    int count = 0;
    int failed = 0;
    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        const std::string name = entry->d_name;
        if (!isPluginSoFile(name)) continue;

        std::string full_path = plugin_dir;
        if (full_path.back() != '/') {
            full_path += '/';
        }
        full_path += name;

        if (loadPlugin(full_path, tool_manager, config_path)) {
            ++count;
        } else {
            ++failed;
        }
    }

    ::closedir(dir);

    if (failed > 0) {
        LOG_WARN("Plugin scan in '{}': {} loaded, {} failed",
                 plugin_dir, count, failed);
    }

    return count;
}

PluginReloadResult PluginManager::reloadFromDirectory(
    const std::string& plugin_dir,
    ToolManager& tool_manager,
    const std::string& config_path) {
    PluginReloadResult result;
    result.unloaded = static_cast<int>(plugins_.size());

    LOG_INFO("Reloading plugins from '{}' (unloading {} plugin(s))",
             plugin_dir, result.unloaded);
    unloadAll(tool_manager);

    result.loaded = loadFromDirectory(plugin_dir, tool_manager, config_path);

    DIR* dir = ::opendir(plugin_dir.c_str());
    if (dir) {
        int failed = 0;
        struct dirent* entry;
        while ((entry = ::readdir(dir)) != nullptr) {
            if (!isPluginSoFile(entry->d_name)) continue;
            ++failed;
        }
        ::closedir(dir);
        result.failed = failed - result.loaded;
        if (result.failed < 0) result.failed = 0;
    }

    LOG_INFO("Plugin reload complete: unloaded={}, loaded={}, failed={}, "
             "tools={}",
             result.unloaded, result.loaded, result.failed, tool_manager.size());
    return result;
}

bool PluginManager::reloadPlugin(const std::string& so_path,
                                  ToolManager& tool_manager,
                                  const std::string& config_path) {
    auto it = std::find_if(plugins_.begin(), plugins_.end(),
        [&](const LoadedPlugin& p) { return p.path == so_path; });

    if (it != plugins_.end()) {
        LOG_INFO("Reloading plugin: {}", so_path);
        unloadPlugin(*it, tool_manager);
        plugins_.erase(it);
    } else {
        LOG_INFO("Loading new plugin: {}", so_path);
    }

    return loadPlugin(so_path, tool_manager, config_path);
}

void PluginManager::unloadPlugin(LoadedPlugin& plugin,
                                  ToolManager& tool_manager) {
    for (const auto& tool_name : plugin.tool_names) {
        tool_manager.unregisterTool(tool_name);
    }
    if (plugin.handle) {
        ::dlclose(plugin.handle);
        plugin.handle = nullptr;
    }
    LOG_INFO("Plugin unloaded: {} ({})", plugin.name, plugin.path);
}

void PluginManager::unloadAll(ToolManager& tool_manager) {
    for (auto& plugin : plugins_) {
        unloadPlugin(plugin, tool_manager);
    }
    plugins_.clear();
}

void PluginManager::unloadHandlesOnly() {
    for (auto& plugin : plugins_) {
        if (plugin.handle) {
            ::dlclose(plugin.handle);
            plugin.handle = nullptr;
        }
    }
    plugins_.clear();
}

bool PluginManager::loadPlugin(const std::string& so_path,
                                ToolManager& tool_manager,
                                const std::string& config_path) {
    for (const auto& plugin : plugins_) {
        if (plugin.path == so_path) {
            LOG_WARN("Plugin already loaded, skip: {}", so_path);
            return false;
        }
    }

    const auto before_names = toolNameSet(tool_manager);

    ::dlerror();
    void* handle = ::dlopen(so_path.c_str(), RTLD_NOW);
    if (!handle) {
        LOG_WARN("Failed to load plugin '{}': {}", so_path, dlErrorString());
        return false;
    }

    if (!config_path.empty()) {
        using SetConfigFunc = void (*)(const char*);
        ::dlerror();
        auto* set_config_fn = reinterpret_cast<SetConfigFunc>(
            ::dlsym(handle, "mcp_plugin_set_config_path"));
        const char* sym_err = ::dlerror();
        if (sym_err) {
            LOG_DEBUG("Plugin '{}': mcp_plugin_set_config_path not available",
                      so_path);
        } else if (set_config_fn) {
            set_config_fn(config_path.c_str());
        }
    }

    using RegisterFunc = int (*)(ToolManager*);

    ::dlerror();
    auto* register_fn = reinterpret_cast<RegisterFunc>(
        ::dlsym(handle, "mcp_plugin_register"));
    if (!register_fn) {
        LOG_WARN("Plugin '{}': missing required symbol mcp_plugin_register: {}",
                 so_path, dlErrorString());
        ::dlclose(handle);
        return false;
    }

    int result = register_fn(&tool_manager);
    if (result != 0) {
        logRegisterFailure(so_path, result);
        ::dlclose(handle);
        return false;
    }

    LoadedPlugin plugin;
    plugin.handle = handle;
    plugin.path = so_path;
    plugin.name = pluginName(handle, so_path);

    for (const auto& info : tool_manager.listTools()) {
        if (!before_names.count(info.name)) {
            plugin.tool_names.push_back(info.name);
        }
    }

    plugins_.push_back(std::move(plugin));

    const auto& loaded = plugins_.back();
    if (!loaded.tool_names.empty()) {
        LOG_INFO("Plugin loaded: {} ({}, {} tool(s) registered)",
                 loaded.name, so_path, loaded.tool_names.size());
    } else {
        LOG_INFO("Plugin loaded: {} ({}, no tools registered — disabled or empty)",
                 loaded.name, so_path);
    }

    return true;
}

} // namespace mcp

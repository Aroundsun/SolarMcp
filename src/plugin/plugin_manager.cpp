#include "mcp/plugin/plugin_manager.h"
#include "mcp/common/macros.h"
#include "mcp/tool/tool_manager.h"
#include "mcp/logger/logger.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <string>

namespace mcp {

namespace {

const char* dlErrorString() {
    const char* err = ::dlerror();
    return (err && *err) ? err : "unknown error";
}

/// 从已加载句柄读取插件名；失败时回退为 .so 路径。
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

} // namespace

// ---------------------------------------------------------------------------
// 析构
// ---------------------------------------------------------------------------

PluginManager::~PluginManager() {
    unloadAll();
}

// ---------------------------------------------------------------------------
// loadFromDirectory
// ---------------------------------------------------------------------------

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
        std::string name = entry->d_name;

        // 仅加载 .so 文件 — 拒绝 foo.so.bak、foo.so.1 等模式
        if (name.size() < 4) continue;
        if (name.compare(name.size() - 3, 3, ".so") != 0) continue;
        if (name.size() > 4 && name[name.size() - 4] == '.') continue;

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

// ---------------------------------------------------------------------------
// unloadAll
// ---------------------------------------------------------------------------

void PluginManager::unloadAll() {
    for (auto* handle : handles_) {
        if (handle) {
            ::dlclose(handle);
        }
    }
    handles_.clear();
}

// ---------------------------------------------------------------------------
// loadPlugin
// ---------------------------------------------------------------------------

bool PluginManager::loadPlugin(const std::string& so_path,
                                ToolManager& tool_manager,
                                const std::string& config_path) {
    const size_t tools_before = tool_manager.size();

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
            // 可选符号，仅 debug 记录
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

    handles_.push_back(handle);

    const std::string name = pluginName(handle, so_path);
    const size_t tools_added = tool_manager.size() - tools_before;
    if (tools_added > 0) {
        LOG_INFO("Plugin loaded: {} ({}, {} tool(s) registered)",
                 name, so_path, tools_added);
    } else {
        LOG_INFO("Plugin loaded: {} ({}, no tools registered — disabled or empty)",
                 name, so_path);
    }

    return true;
}

} // namespace mcp

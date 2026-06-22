#include "mcp/plugin/plugin_manager.h"
#include "mcp/plugin/callback_tool.h"
#include "mcp/plugin/plugin_abi.h"
#include "mcp/common/macros.h"
#include "mcp/tool/tool_manager.h"
#include "mcp/logger/logger.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <string>
#include <vector>

namespace mcp {

namespace {

const char* dlErrorString() {
    const char* err = ::dlerror();
    return (err && *err) ? err : "unknown error";
}

bool isPluginSoFile(const std::string& name) {
    if (name.size() < 4) return false;
    if (name.compare(name.size() - 3, 3, ".so") != 0) return false;
    if (name.size() > 4 && name[name.size() - 4] == '.') return false;
    return true;
}

struct HostState {
    ToolManager* tool_manager = nullptr;
    std::string config_path;
    std::vector<std::string>* pending_tools = nullptr;
};

int hostRegisterTool(void* host_ctx,
                     const char* name,
                     const char* description,
                     const char* input_schema_json,
                     mcp_tool_execute_fn execute) {
    auto* state = static_cast<HostState*>(host_ctx);
    if (!state || !state->tool_manager || !name || !execute) {
        return MCP_PLUGIN_ERR_HOST;
    }

    auto tool = std::make_unique<CallbackTool>(
        name,
        description ? description : "",
        input_schema_json ? input_schema_json : "{}",
        execute);

    if (!state->tool_manager->registerTool(std::move(tool))) {
        return MCP_PLUGIN_ERR_DUP;
    }

    if (state->pending_tools) {
        state->pending_tools->push_back(name);
    }
    return MCP_PLUGIN_OK;
}

void hostLog(void* /*host_ctx*/, int level, const char* message) {
    if (!message) return;
    switch (level) {
    case MCP_LOG_TRACE: LOG_TRACE("{}", message); break;
    case MCP_LOG_DEBUG: LOG_DEBUG("{}", message); break;
    case MCP_LOG_INFO:  LOG_INFO("{}", message); break;
    case MCP_LOG_WARN:  LOG_WARN("{}", message); break;
    case MCP_LOG_ERROR: LOG_ERROR("{}", message); break;
    default: LOG_INFO("{}", message); break;
    }
}

const char* hostGetConfigPath(void* host_ctx) {
    auto* state = static_cast<HostState*>(host_ctx);
    return state ? state->config_path.c_str() : "";
}

mcp_host_api makeHostApi(HostState* state) {
    mcp_host_api api{};
    api.abi_version = SOLARMCP_PLUGIN_ABI;
    api.host_ctx = state;
    api.register_tool = hostRegisterTool;
    api.log = hostLog;
    api.get_config_path = hostGetConfigPath;
    return api;
}

using AbiVersionFn = int (*)();
using VersionFn = const char* (*)();
using NameFn = const char* (*)();
using InitFn = int (*)(const mcp_host_api*);
using RegisterFn = int (*)();
using ShutdownFn = void (*)();

struct PluginSymbols {
    AbiVersionFn abi_version = nullptr;
    VersionFn version = nullptr;
    NameFn name = nullptr;
    InitFn init = nullptr;
    RegisterFn register_fn = nullptr;
    ShutdownFn shutdown = nullptr;
};

bool resolveSymbols(void* handle, PluginSymbols& sym) {
    sym.abi_version = reinterpret_cast<AbiVersionFn>(
        ::dlsym(handle, "mcp_plugin_abi_version"));
    sym.version = reinterpret_cast<VersionFn>(
        ::dlsym(handle, "mcp_plugin_version"));
    sym.name = reinterpret_cast<NameFn>(
        ::dlsym(handle, "mcp_plugin_name"));
    sym.init = reinterpret_cast<InitFn>(
        ::dlsym(handle, "mcp_plugin_init"));
    sym.register_fn = reinterpret_cast<RegisterFn>(
        ::dlsym(handle, "mcp_plugin_register"));
    sym.shutdown = reinterpret_cast<ShutdownFn>(
        ::dlsym(handle, "mcp_plugin_shutdown"));
    return sym.abi_version && sym.version && sym.name &&
           sym.init && sym.register_fn;
}

void logRegisterFailure(const std::string& so_path, int code) {
    switch (code) {
    case MCP_PLUGIN_ERR_HOST:
        LOG_ERROR("Plugin {}: init/register failed — invalid host", so_path);
        break;
    case MCP_PLUGIN_ERR_DUP:
        LOG_WARN("Plugin {}: tool name already registered (duplicate)", so_path);
        break;
    default:
        LOG_WARN("Plugin {}: register returned error code {}", so_path, code);
        break;
    }
}

void shutdownAndClose(void* handle, PluginSymbols& sym) {
    if (sym.shutdown) {
        sym.shutdown();
    }
    if (handle) {
        ::dlclose(handle);
    }
}

bool activatePlugin(PluginSymbols& sym,
                    HostState& host_state,
                    const std::string& so_path,
                    std::vector<std::string>& out_tool_names,
                    std::string& out_name,
                    std::string& out_version,
                    int& out_abi) {
    out_tool_names.clear();

    int abi = sym.abi_version();
    if (abi != SOLARMCP_PLUGIN_ABI) {
        LOG_WARN("Plugin '{}': ABI mismatch (plugin={}, host={})",
                 so_path, abi, SOLARMCP_PLUGIN_ABI);
        return false;
    }

    const char* ver = sym.version();
    const char* pname = sym.name();
    out_version = (ver && *ver) ? ver : "unknown";
    out_name = (pname && *pname) ? pname : so_path;
    out_abi = abi;

    LOG_INFO("Plugin '{}': abi={}, version={}, name={}",
             so_path, abi, out_version, out_name);

    host_state.pending_tools = &out_tool_names;
    mcp_host_api api = makeHostApi(&host_state);

    if (sym.init(&api) != MCP_PLUGIN_OK) {
        LOG_WARN("Plugin '{}': mcp_plugin_init failed", so_path);
        return false;
    }

    int rc = sym.register_fn();
    host_state.pending_tools = nullptr;

    if (rc != MCP_PLUGIN_OK) {
        logRegisterFailure(so_path, rc);
        if (sym.shutdown) sym.shutdown();
        return false;
    }

    return true;
}

std::vector<std::string> scanPluginPaths(const std::string& plugin_dir) {
    std::vector<std::string> paths;
    DIR* dir = ::opendir(plugin_dir.c_str());
    if (!dir) return paths;

    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        const std::string name = entry->d_name;
        if (!isPluginSoFile(name)) continue;

        std::string full_path = plugin_dir;
        if (full_path.back() != '/') full_path += '/';
        full_path += name;
        paths.push_back(std::move(full_path));
    }
    ::closedir(dir);
    std::sort(paths.begin(), paths.end());
    return paths;
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
    ::closedir(dir);

    int count = 0;
    int failed = 0;
    for (const auto& path : scanPluginPaths(plugin_dir)) {
        if (loadPlugin(path, tool_manager, config_path)) {
            ++count;
        } else {
            ++failed;
        }
    }

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

    auto paths = scanPluginPaths(plugin_dir);
    ToolManager trial_tm;
    for (const auto& path : paths) {
        if (!validatePlugin(path, trial_tm, config_path)) {
            LOG_WARN("Plugin reload aborted: validation failed for '{}'", path);
            for (const auto& name : trial_tm.listTools()) {
                trial_tm.unregisterTool(name.name);
            }
            result.failed = static_cast<int>(paths.size());
            return result;
        }
        trial_tm.clear();
    }

    LOG_INFO("Reloading plugins from '{}' ({} validated)", plugin_dir, paths.size());
    unloadAll(tool_manager);

    for (const auto& path : paths) {
        if (loadPlugin(path, tool_manager, config_path)) {
            ++result.loaded;
        } else {
            ++result.failed;
        }
    }

    LOG_INFO("Plugin reload complete: unloaded={}, loaded={}, failed={}, tools={}",
             result.unloaded, result.loaded, result.failed, tool_manager.size());
    return result;
}

bool PluginManager::reloadPlugin(const std::string& so_path,
                                  ToolManager& tool_manager,
                                  const std::string& config_path) {
    ToolManager trial_tm;
    if (!validatePlugin(so_path, trial_tm, config_path)) {
        LOG_WARN("Plugin reload rejected: '{}' failed ABI validation", so_path);
        for (const auto& t : trial_tm.listTools()) {
            trial_tm.unregisterTool(t.name);
        }
        return false;
    }
    trial_tm.clear();

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
    if (plugin.handle) {
        PluginSymbols sym{};
        sym.shutdown = reinterpret_cast<ShutdownFn>(
            ::dlsym(plugin.handle, "mcp_plugin_shutdown"));
        if (sym.shutdown) {
            sym.shutdown();
        }
    }

    for (const auto& tool_name : plugin.tool_names) {
        tool_manager.unregisterTool(tool_name);
    }
    if (plugin.handle) {
        ::dlclose(plugin.handle);
        plugin.handle = nullptr;
    }
    LOG_INFO("Plugin unloaded: {} v{} ({})",
             plugin.name, plugin.version, plugin.path);
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
            auto shutdown = reinterpret_cast<ShutdownFn>(
                ::dlsym(plugin.handle, "mcp_plugin_shutdown"));
            if (shutdown) {
                shutdown();
            }
            ::dlclose(plugin.handle);
            plugin.handle = nullptr;
        }
    }
    plugins_.clear();
}

bool PluginManager::validatePlugin(const std::string& so_path,
                                    ToolManager& trial_tool_manager,
                                    const std::string& config_path) {
    ::dlerror();
    void* handle = ::dlopen(so_path.c_str(), RTLD_NOW);
    if (!handle) {
        LOG_WARN("Plugin validate '{}': dlopen failed: {}",
                 so_path, dlErrorString());
        return false;
    }

    PluginSymbols symbols{};
    if (!resolveSymbols(handle, symbols)) {
        LOG_WARN("Plugin validate '{}': missing required ABI symbols: {}",
                 so_path, dlErrorString());
        ::dlclose(handle);
        return false;
    }

    HostState host_state;
    host_state.tool_manager = &trial_tool_manager;
    host_state.config_path = config_path;

    std::vector<std::string> tool_names;
    std::string name;
    std::string version;
    int abi = 0;

    bool ok = activatePlugin(symbols, host_state, so_path,
                             tool_names, name, version, abi);

    for (const auto& tool_name : tool_names) {
        trial_tool_manager.unregisterTool(tool_name);
    }
    shutdownAndClose(handle, symbols);
    return ok;
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

    ::dlerror();
    void* handle = ::dlopen(so_path.c_str(), RTLD_NOW);
    if (!handle) {
        LOG_WARN("Failed to load plugin '{}': {}", so_path, dlErrorString());
        return false;
    }

    PluginSymbols symbols{};
    if (!resolveSymbols(handle, symbols)) {
        LOG_WARN("Plugin '{}': missing required ABI symbols: {}",
                 so_path, dlErrorString());
        ::dlclose(handle);
        return false;
    }

    HostState host_state;
    host_state.tool_manager = &tool_manager;
    host_state.config_path = config_path;

    LoadedPlugin plugin;
    plugin.handle = handle;
    plugin.path = so_path;

    if (!activatePlugin(symbols, host_state, so_path,
                        plugin.tool_names, plugin.name, plugin.version,
                        plugin.abi_version)) {
        shutdownAndClose(handle, symbols);
        return false;
    }

    plugins_.push_back(std::move(plugin));
    const auto& loaded = plugins_.back();

    if (!loaded.tool_names.empty()) {
        LOG_INFO("Plugin loaded: {} v{} (abi {}, {} tool(s))",
                 loaded.name, loaded.version, loaded.abi_version,
                 loaded.tool_names.size());
    } else {
        LOG_INFO("Plugin loaded: {} v{} (abi {}, no tools — disabled or empty)",
                 loaded.name, loaded.version, loaded.abi_version);
    }
    return true;
}

} // namespace mcp

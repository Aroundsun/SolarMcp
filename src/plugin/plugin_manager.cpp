#include "mcp/plugin/plugin_manager.h"
#include "mcp/tool/tool_manager.h"

#include <cstring>
#include <dirent.h>
#include <dlfcn.h>

namespace mcp {

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
        return 0; // 目录不存在或无法打开
    }

    int count = 0;
    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        std::string name = entry->d_name;

        // 仅加载 .so 文件 — 拒绝 foo.so.bak、foo.so.1 等模式
        // 检查：名称以 ".so" 结尾，且其前一个字符不是 '.'
        if (name.size() < 4) continue;
        if (name.compare(name.size() - 3, 3, ".so") != 0) continue;
        // 拒绝双扩展名如 foo.so.bak（".so" 前一个字符为 '.'）
        if (name.size() > 4 && name[name.size() - 4] == '.') continue;

        std::string full_path = plugin_dir;
        if (full_path.back() != '/') {
            full_path += '/';
        }
        full_path += name;

        if (loadPlugin(full_path, tool_manager, config_path)) {
            ++count;
        }
    }

    ::closedir(dir);
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
    // 清除已有错误
    ::dlerror();

    void* handle = ::dlopen(so_path.c_str(), RTLD_NOW);
    if (!handle) {
        const char* err = ::dlerror();
        if (err) {
            // 由调用方记录日志
        }
        return false;
    }

    // 可选：传递配置文件路径
    if (!config_path.empty()) {
        using SetConfigFunc = void (*)(const char*);
        auto* set_config_fn = reinterpret_cast<SetConfigFunc>(
            ::dlsym(handle, "mcp_plugin_set_config_path"));
        if (set_config_fn) {
            set_config_fn(config_path.c_str());
        }
    }

    // 查找必需的符号
    using RegisterFunc = int (*)(ToolManager*);

    auto* register_fn = reinterpret_cast<RegisterFunc>(
        ::dlsym(handle, "mcp_plugin_register"));

    if (!register_fn) {
        ::dlclose(handle);
        return false;
    }

    // 调用注册函数
    int result = register_fn(&tool_manager);
    if (result != 0) {
        ::dlclose(handle);
        return false;
    }

    handles_.push_back(handle);
    return true;
}

} // namespace mcp

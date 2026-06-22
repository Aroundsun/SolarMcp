#pragma once

#include "mcp/common/noncopyable.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// yaml-cpp 前向声明
namespace YAML {
class Node;
}

namespace mcp {

/// 单例配置管理器。
///
/// 启动时加载 YAML（或 JSON）配置文件，并提供类型安全的访问器。
/// 内部所有值均以 nlohmann::json 存储。
///
/// 线程安全：loadFile() 完成后只读，请求处理阶段无需加锁。
class ConfigManager : public NonCopyable {
public:
    /// 获取全局 ConfigManager 实例。
    static ConfigManager& getInstance();

    /// 加载并解析配置文件。
    /// 支持 YAML（通过 yaml-cpp）和 JSON（通过 nlohmann/json）。
    /// @param file_path  配置文件路径
    /// @return 成功返回 true，文件无法加载返回 false
    bool loadFile(const std::string& file_path);

    /// 通过点分隔键访问底层 json 对象。
    /// 例如 get("server.host") 返回 data_["server"]["host"] 的值
    const nlohmann::json& get(const std::string& key) const;

    /// 获取字符串值，键缺失时返回 `default_val`。
    std::string getString(const std::string& key,
                          const std::string& default_val = "") const;

    /// 获取整数值，键缺失时返回 `default_val`。
    int getInt(const std::string& key, int default_val = 0) const;

    /// 获取布尔值，键缺失时返回 `default_val`。
    bool getBool(const std::string& key, bool default_val = false) const;

    /// 获取字符串数组，缺失时返回空 vector。
    std::vector<std::string> getStringArray(const std::string& key) const;

    /// 给定键存在于配置中时返回 true。
    bool has(const std::string& key) const;

    /// 返回已加载配置文件的路径（未加载时为空）。
    const std::string& filePath() const noexcept { return file_path_; }

private:
    ConfigManager() = default;

    /// 将点分隔路径解析为 json 引用。
    /// 任一段缺失时返回 nullptr。
    const nlohmann::json* resolve(const std::string& key) const;

    /// 递归将 YAML::Node 转换为 nlohmann::json。
    static nlohmann::json yamlToJson(const YAML::Node& node);

    nlohmann::json data_;
    std::string file_path_;
};

} // namespace mcp

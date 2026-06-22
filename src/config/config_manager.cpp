#include "mcp/config/config_manager.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <yaml-cpp/yaml.h>

namespace mcp {

// ---------------------------------------------------------------------------
// 单例访问
// ---------------------------------------------------------------------------

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

// ---------------------------------------------------------------------------
// loadFile — 检测格式（YAML 或 JSON）并解析
// ---------------------------------------------------------------------------

bool ConfigManager::loadFile(const std::string& file_path) {
    std::ifstream infile(file_path);
    if (!infile.is_open()) {
        std::cerr << "[ConfigManager] Cannot open config file: "
                  << file_path << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << infile.rdbuf();
    std::string content = buffer.str();
    infile.close();

    // 按文件扩展名检测格式
    bool is_yaml = false;
    if (file_path.size() >= 5 &&
        file_path.compare(file_path.size() - 5, 5, ".yaml") == 0) {
        is_yaml = true;
    } else if (file_path.size() >= 4 &&
               file_path.compare(file_path.size() - 4, 4, ".yml") == 0) {
        is_yaml = true;
    }

    try {
        if (is_yaml) {
            YAML::Node yaml_root = YAML::Load(content);
            // 通过递归辅助函数将 YAML 转为 nlohmann::json
            data_ = yamlToJson(yaml_root);
        } else {
            data_ = nlohmann::json::parse(content);
        }
    } catch (const YAML::Exception& e) {
        std::cerr << "[ConfigManager] YAML parse error: " << e.what()
                  << std::endl;
        return false;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[ConfigManager] JSON parse error: " << e.what()
                  << std::endl;
        return false;
    }

    file_path_ = file_path;
    std::cerr << "[ConfigManager] Loaded config: " << file_path << std::endl;
    return true;
}

// ---------------------------------------------------------------------------
// yamlToJson — 递归 YAML → JSON 转换辅助函数
// ---------------------------------------------------------------------------

nlohmann::json ConfigManager::yamlToJson(const YAML::Node& node) {
    if (node.IsNull()) {
        return nullptr;
    }
    if (node.IsScalar()) {
        // 先尝试 int，再 double，最后 string
        std::string scalar = node.as<std::string>();
        try {
            // 整数检测
            size_t pos = 0;
            int int_val = std::stoi(scalar, &pos);
            if (pos == scalar.size()) {
                return int_val;
            }
            // 浮点数检测
            double dbl_val = std::stod(scalar, &pos);
            if (pos == scalar.size()) {
                return dbl_val;
            }
        } catch (...) {
            // 回落为字符串
        }
        // 布尔检测
        if (scalar == "true" || scalar == "True" || scalar == "TRUE") {
            return true;
        }
        if (scalar == "false" || scalar == "False" || scalar == "FALSE") {
            return false;
        }
        return scalar;
    }
    if (node.IsSequence()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& item : node) {
            arr.push_back(yamlToJson(item));
        }
        return arr;
    }
    if (node.IsMap()) {
        nlohmann::json obj = nlohmann::json::object();
        for (const auto& kv : node) {
            obj[kv.first.as<std::string>()] = yamlToJson(kv.second);
        }
        return obj;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// resolve — 遍历点分隔路径
// ---------------------------------------------------------------------------

const nlohmann::json* ConfigManager::resolve(const std::string& key) const {
    const nlohmann::json* current = &data_;
    std::string segment;
    std::istringstream stream(key);

    while (std::getline(stream, segment, '.')) {
        if (!current->is_object() || !current->contains(segment)) {
            return nullptr;
        }
        current = &(*current)[segment];
    }
    return current;
}

// ---------------------------------------------------------------------------
// 访问器
// ---------------------------------------------------------------------------

const nlohmann::json& ConfigManager::get(const std::string& key) const {
    static const nlohmann::json kEmpty;
    const auto* ptr = resolve(key);
    return ptr ? *ptr : kEmpty;
}

std::string ConfigManager::getString(const std::string& key,
                                     const std::string& default_val) const {
    const auto* ptr = resolve(key);
    if (ptr && ptr->is_string()) {
        return ptr->get<std::string>();
    }
    return default_val;
}

int ConfigManager::getInt(const std::string& key, int default_val) const {
    const auto* ptr = resolve(key);
    if (ptr && ptr->is_number_integer()) {
        return ptr->get<int>();
    }
    return default_val;
}

bool ConfigManager::getBool(const std::string& key, bool default_val) const {
    const auto* ptr = resolve(key);
    if (ptr && ptr->is_boolean()) {
        return ptr->get<bool>();
    }
    return default_val;
}

std::vector<std::string> ConfigManager::getStringArray(
    const std::string& key) const {
    std::vector<std::string> result;
    const auto* ptr = resolve(key);
    if (ptr && ptr->is_array()) {
        for (const auto& item : *ptr) {
            if (item.is_string()) {
                result.push_back(item.get<std::string>());
            }
        }
    }
    return result;
}

bool ConfigManager::has(const std::string& key) const {
    return resolve(key) != nullptr;
}

} // namespace mcp

#include "mcp/tool/read_file_tool.h"
#include "mcp/common/types.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <limits.h>

namespace mcp {

// ---------------------------------------------------------------------------
// 构造
// ---------------------------------------------------------------------------

ReadFileTool::ReadFileTool(size_t max_size_bytes,
                           std::vector<std::string> allowed_paths)
    : max_size_bytes_(max_size_bytes)
    , allowed_paths_(std::move(allowed_paths))
{}

// ---------------------------------------------------------------------------
// inputSchema
// ---------------------------------------------------------------------------

nlohmann::json ReadFileTool::inputSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {
                {"type", "string"},
                {"description", "Absolute path to the file to read"}
            }}
        }},
        {"required", nlohmann::json::array({"path"})}
    };
}

// ---------------------------------------------------------------------------
// execute
// ---------------------------------------------------------------------------

Result ReadFileTool::execute(const nlohmann::json& params, Context& /*ctx*/) {
    // 校验必需参数
    if (!params.contains("path") || !params["path"].is_string()) {
        return Result::err(ErrorCodes::kInvalidParams,
                           "Missing required parameter: 'path' (string)");
    }

    std::string path = params["path"].get<std::string>();

    // 安全：在校验允许目录前先解析真实路径。
    // realpath() 解析符号链接和 ".." 组件，防止如
    // /tmp/../../etc/passwd 的目录遍历攻击。
    char resolved[PATH_MAX];
    if (::realpath(path.c_str(), resolved) == nullptr) {
        return Result::err(ErrorCodes::kToolError,
                           "Cannot resolve path: " + path);
    }

    // 安全：对照允许前缀校验解析后的路径
    if (!isPathAllowed(resolved)) {
        return Result::err(ErrorCodes::kToolError,
                           "Access denied: path is not in allowed directories");
    }

    // 使用解析后的路径打开文件
    std::ifstream file(resolved, std::ios::binary);
    if (!file.is_open()) {
        return Result::err(ErrorCodes::kToolError,
                           "Cannot open file: " + std::string(resolved));
    }

    // 获取文件大小
    file.seekg(0, std::ios::end);
    size_t file_size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    if (file_size > max_size_bytes_) {
        return Result::err(ErrorCodes::kToolError,
                           "File too large: " + std::to_string(file_size) +
                           " bytes (max: " + std::to_string(max_size_bytes_) + ")");
    }

    // 读取文件内容
    std::string content(file_size, '\0');
    file.read(content.data(), static_cast<std::streamsize>(file_size));

    if (file.fail() && !file.eof()) {
        return Result::err(ErrorCodes::kToolError,
                           "Error reading file: " + path);
    }

    // 构建符合 MCP 的结果
    nlohmann::json result = {
        {"content", nlohmann::json::array({
            {{"type", "text"}, {"text", content}}
        })}
    };

    return Result::ok(std::move(result));
}

// ---------------------------------------------------------------------------
// isPathAllowed
// ---------------------------------------------------------------------------

bool ReadFileTool::isPathAllowed(const std::string& path) const {
    // 路径必须为绝对路径
    if (path.empty() || path[0] != '/') {
        return false;
    }

    for (const auto& prefix : allowed_paths_) {
        if (path.size() >= prefix.size() &&
            path.compare(0, prefix.size(), prefix) == 0) {
            // 边界检查：前缀后下一字符须为
            // '/' 或字符串结尾，防止 /tmpA 匹配允许前缀 /tmp。
            if (path.size() == prefix.size() || path[prefix.size()] == '/') {
                return true;
            }
        }
    }
    return false;
}

} // namespace mcp

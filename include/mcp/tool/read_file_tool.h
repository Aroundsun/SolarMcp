#pragma once

#include "mcp/tool/tool.h"

#include <string>
#include <vector>

namespace mcp {

/// 读取文件内容的内置工具。
///
/// 支持可配置的最大文件大小和允许路径前缀白名单，用于安全控制。
class ReadFileTool : public Tool {
public:
    /// 带安全约束构造。
    /// @param max_size_bytes   最大可读文件大小（默认：10 MB）
    /// @param allowed_paths    允许的路径前缀（默认：{"/tmp", "/home"}）
    ReadFileTool(size_t max_size_bytes = 10 * 1024 * 1024,
                 std::vector<std::string> allowed_paths = {"/tmp", "/home"});

    std::string name() const override { return "read_file"; }

    std::string description() const override {
        return "Read the complete contents of a file from the file system. "
               "Handles various text encodings and binary files.";
    }

    nlohmann::json inputSchema() const override;

    Result execute(const nlohmann::json& params, Context& ctx) override;

private:
    /// 校验请求路径是否在允许目录内。
    bool isPathAllowed(const std::string& path) const;

    size_t max_size_bytes_;
    std::vector<std::string> allowed_paths_;
};

} // namespace mcp

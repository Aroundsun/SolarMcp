#pragma once

#include "mcp/tool/tool.h"
#include "mcp/common/types.h"

#include <string>
#include <vector>

namespace mcp {

/// 执行 Shell 命令的工具。
///
/// 安全性：allowed_shells_ 仅限制使用的 shell 解释器路径（默认 /bin/sh、/bin/bash），
/// 不限制 command 内容。启用前请确保已开启 Token 认证，并仅在受信环境使用。
///
/// 参数：
///   - command (string, 必填): 要执行的命令
///   - timeout_seconds (int, 可选): 超时秒数，默认 30
///
/// 返回：
///   - stdout (string): 标准输出
///   - stderr (string): 标准错误输出
///   - exit_code (int): 进程退出码（非零时 status 为 "error"，RPC 层仍成功）
///   - status (string): "success" 或 "error"
class ShellTool : public Tool {
public:
    /// @param timeout_sec    默认超时时间（秒）
    /// @param allowed_shells 允许使用的 shell 解释器路径
    /// @param max_output_bytes stdout 与 stderr 合计最大字节数
    explicit ShellTool(int timeout_sec = 30,
                       std::vector<std::string> allowed_shells = {"/bin/sh", "/bin/bash"},
                       size_t max_output_bytes = 10 * 1024 * 1024);

    std::string name() const override;
    std::string description() const override;
    nlohmann::json inputSchema() const override;
    Result execute(const nlohmann::json& params, Context& ctx) override;

private:
    struct CommandOutput {
        int exit_code = -1;
        std::string stdout_data;
        std::string stderr_data;
    };

    /// fork + exec 执行命令，poll 边读边等，避免管道死锁。
    CommandOutput runCommand(const std::string& command, int timeout_seconds,
                             size_t max_bytes) const;

    int default_timeout_sec_;
    std::vector<std::string> allowed_shells_;
    size_t max_output_bytes_;
};

} // namespace mcp

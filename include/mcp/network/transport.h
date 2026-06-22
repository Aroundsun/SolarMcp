#pragma once

#include <string>

namespace mcp {

/// 抽象传输层接口。
///
/// 将 TcpServer 与具体 socket 类型解耦，
/// 便于未来支持 Unix Domain Socket 或其他传输方式。
class Transport {
public:
    virtual ~Transport() = default;

    /// 开始监听入站连接。
    /// @return 成功返回 true
    virtual bool listen() = 0;

    /// 接受入站连接。
    /// @return 成功返回文件描述符，错误返回 -1
    virtual int accept() = 0;

    /// 关闭传输层。
    virtual void close() = 0;

    /// 可读的传输类型标识（如 "tcp"、"unix"）。
    virtual std::string type() const = 0;
};

} // namespace mcp

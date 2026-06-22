#pragma once

#include <netinet/in.h>
#include <string>

namespace mcp {

/// sockaddr_in 的轻量包装。
///
/// 提供可读的 IP/端口访问器，以及 POSIX socket API 所需的
/// 原始 sockaddr 结构转换。
class InetAddress {
public:
    /// 从端口和 IP 字符串构造（默认："0.0.0.0"）。
    /// @param port  主机字节序端口号
    /// @param ip    IPv4 地址字符串，如 "127.0.0.1"
    InetAddress(uint16_t port = 0, const std::string& ip = "0.0.0.0");

    /// 从已有 sockaddr_in 构造（如 accept 之后）。
    explicit InetAddress(const sockaddr_in& addr) : addr_(addr) {}

    /// 点分四段 IP 地址字符串。
    std::string toIp() const;

    /// "IP:port" 字符串，如 "127.0.0.1:8090"。
    std::string toIpPort() const;

    /// 主机字节序端口号。
    uint16_t port() const noexcept { return ntohs(addr_.sin_port); }

    /// 系统调用用的原始 sockaddr 指针。
    const sockaddr* sockAddr() const {
        return reinterpret_cast<const sockaddr*>(&addr_);
    }

    /// 可变的 sockaddr 指针。
    sockaddr* sockAddr() {
        return reinterpret_cast<sockaddr*>(&addr_);
    }

    /// sockaddr 结构大小。
    socklen_t sockLen() const noexcept { return sizeof(addr_); }

private:
    sockaddr_in addr_{};
};

} // namespace mcp

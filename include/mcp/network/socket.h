#pragma once

#include "mcp/network/inet_address.h"
#include "mcp/common/noncopyable.h"

namespace mcp {

/// POSIX socket 文件描述符的 RAII 包装。
///
/// 持有 fd，析构时自动调用 close()。
/// 提供 bind/listen/accept/setNonBlock 便捷方法。
class Socket : public NonCopyable {
public:
    /// 创建未初始化的 socket（fd = -1）。
    Socket();

    /// 接管已有 fd 的所有权。
    explicit Socket(int fd) noexcept;

    /// 析构时关闭 socket。
    ~Socket();

    // 仅可移动
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    /// 返回原始文件描述符。
    int fd() const noexcept { return fd_; }

    /// 创建 TCP socket（SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC）。
    void createTcp();

    /// 绑定到给定地址。
    void bind(const InetAddress& addr);

    /// 以给定 backlog 开始监听。
    void listen(int backlog = 128);

    /// 接受新连接，返回客户端 fd 并填充 `peer_addr`。
    int accept(InetAddress* peer_addr);

    /// 在 socket 上设置 SO_REUSEADDR。
    void setReuseAddr(bool on);

    /// 在 socket 上设置 O_NONBLOCK。
    void setNonBlock(bool on);

    /// 关闭连接写端（TCP 半关闭）。
    void shutdownWrite();

    /// fd 有效（>= 0）时返回 true。
    bool valid() const noexcept { return fd_ >= 0; }

private:
    int fd_{-1};
};

} // namespace mcp

#pragma once

#include "mcp/common/noncopyable.h"
#include "mcp/network/buffer.h"
#include "mcp/network/inet_address.h"
#include "mcp/protocol/message.h"

#include <functional>
#include <memory>
#include <string>

namespace mcp {

class EventLoop;
class Channel;
class Socket;
class Codec;

/// 表示与客户端的单条 TCP 连接。
///
/// 拥有 socket fd、输入 Buffer、输出 Buffer、用于消息分帧的 Codec，
/// 以及向 EventLoop 注册的 Channel。
///
/// 从 socket 读入输入缓冲区，用 Codec 提取完整 JSON-RPC 消息，
/// 并通过回调分发。
///
/// 线程安全：所有方法必须在所属 EventLoop 线程调用。
class TcpConnection : public NonCopyable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
    using MessageCallback = std::function<void(
        TcpConnection* conn, const Message& message)>;

    using CloseCallback = std::function<void(TcpConnection* conn)>;

    /// 连接生命周期状态。
    enum class State {
        kConnecting,
        kConnected,
        kDisconnecting,
        kDisconnected,
    };

    TcpConnection(EventLoop* loop,
                  const std::string& name,
                  int sock_fd,
                  const InetAddress& local_addr,
                  const InetAddress& peer_addr);
    ~TcpConnection();

    /// 开始监控 I/O 事件，必须在 loop 线程调用。
    void connectEstablished();

    /// 发起优雅关闭（半关闭 + 禁用 channel）。
    void shutdown();

    /// 通过输出缓冲区发送 Message（由 codec 编码）。
    void send(const Message& message);

    /// 发送原始字符串（已编码）。
    void send(const std::string& data);

    /// 发送原始字节。
    void send(const char* data, size_t len);

    // ---- 编解码器 ----

    /// 设置此连接的协议编解码器。
    void setCodec(std::unique_ptr<Codec> codec);

    // ---- 回调设置 ----

    void setMessageCallback(MessageCallback cb) {
        message_callback_ = std::move(cb);
    }

    void setCloseCallback(CloseCallback cb) {
        close_callback_ = std::move(cb);
    }

    // ---- 访问器 ----

    const std::string& name() const noexcept { return name_; }
    EventLoop* loop() const noexcept { return loop_; }
    State state() const noexcept { return state_; }
    const InetAddress& peerAddress() const noexcept { return peer_addr_; }

private:
    /// Channel 在有数据可读时调用。
    void handleRead();

    /// Channel 在 socket 可写时调用。
    void handleWrite();

    /// Channel 在错误或挂断时调用。
    void handleClose();

    /// Channel 在 epoll 错误时调用。
    void handleError();

    /// 处理输入缓冲区以查找完整消息。
    /// 使用 Codec 进行基于 Content-Length 的分帧。
    void processInput();

    EventLoop* loop_;
    std::string name_;
    State state_{State::kConnecting};

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    InetAddress local_addr_;
    InetAddress peer_addr_;

    Buffer input_buffer_;
    Buffer output_buffer_;

    std::unique_ptr<Codec> codec_;

    MessageCallback message_callback_;
    CloseCallback close_callback_;
};

} // namespace mcp

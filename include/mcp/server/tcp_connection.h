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
/// 拥有 socket fd、输入/输出 Buffer、Codec 及 Channel。
/// 所有 I/O 操作须在所属 EventLoop 线程执行；跨线程发送请用 sendInLoop()。
class TcpConnection : public NonCopyable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
    using MessageCallback = std::function<void(
        const std::shared_ptr<TcpConnection>& conn, const Message& message)>;

    using CloseCallback = std::function<void(
        const std::shared_ptr<TcpConnection>& conn)>;

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

    void connectEstablished();
    void shutdown();

    void send(const Message& message);
    void send(const std::string& data);
    void send(const char* data, size_t len);

    /// 线程安全：在所属 EventLoop 线程发送 Message。
    void sendInLoop(const Message& message);

    void setCodec(std::unique_ptr<Codec> codec);

    void setAuthenticated(bool authenticated) {
        authenticated_ = authenticated;
    }

    bool isAuthenticated() const noexcept { return authenticated_; }

    void setMessageCallback(MessageCallback cb) {
        message_callback_ = std::move(cb);
    }

    void setCloseCallback(CloseCallback cb) {
        close_callback_ = std::move(cb);
    }

    const std::string& name() const noexcept { return name_; }
    EventLoop* loop() const noexcept { return loop_; }
    State state() const noexcept { return state_; }
    const InetAddress& peerAddress() const noexcept { return peer_addr_; }

private:
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();
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

    bool authenticated_{false};
};

} // namespace mcp

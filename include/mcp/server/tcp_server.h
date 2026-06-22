#pragma once

#include "mcp/common/noncopyable.h"
#include "mcp/network/inet_address.h"
#include "mcp/protocol/message.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace mcp {

class EventLoop;
class TcpConnection;
class Channel;
class Socket;
class ThreadPool;
class Dispatcher;
class ToolManager;

/// 基于 Reactor 模式的高级 TCP 服务器。
///
/// 在指定地址监听、接受连接，并为每个客户端创建
/// TcpConnection。与 Dispatcher 集成进行 JSON-RPC 方法路由，
/// 与 ToolManager 集成进行工具执行。
class TcpServer : public NonCopyable {
public:
    using MessageCallback = std::function<void(
        TcpConnection* conn, const Message& message)>;

    using ConnectionCallback = std::function<void(TcpConnection* conn)>;

    TcpServer(EventLoop* loop, const InetAddress& listen_addr);
    ~TcpServer();

    /// 开始监听连接。
    void start();

    /// 停止服务器并优雅关闭所有连接。
    void stop();

    /// 设置收到完整消息时调用的回调。
    void setMessageCallback(MessageCallback cb) {
        message_callback_ = std::move(cb);
    }

    /// 设置新连接建立时调用的回调。
    void setConnectionCallback(ConnectionCallback cb) {
        connection_callback_ = std::move(cb);
    }

    /// 设置连接关闭时调用的回调。
    void setCloseCallback(ConnectionCallback cb) {
        close_callback_ = std::move(cb);
    }

    /// 设置用于异步任务执行的线程池（P1）。
    void setThreadPool(std::shared_ptr<ThreadPool> pool) {
        thread_pool_ = std::move(pool);
    }

    /// 设置 JSON-RPC 方法路由的分发器。
    void setDispatcher(Dispatcher* dispatcher) {
        dispatcher_ = dispatcher;
    }

    /// 设置工具执行的工具管理器。
    void setToolManager(ToolManager* tool_manager) {
        tool_manager_ = tool_manager;
    }

    /// 返回所属的 EventLoop。
    EventLoop* loop() const noexcept { return loop_; }

    /// 返回分发器（未设置时可能为 nullptr）。
    Dispatcher* dispatcher() const noexcept { return dispatcher_; }

    /// 返回工具管理器（未设置时可能为 nullptr）。
    ToolManager* toolManager() const noexcept { return tool_manager_; }

private:
    /// acceptor channel 在新连接到达时调用。
    void onNewConnection(int sock_fd, const InetAddress& peer_addr);

    /// TcpConnection 销毁时调用。
    void removeConnection(TcpConnection* conn);

    /// 默认消息处理：decode → dispatch → encode → reply。
    void onMessage(TcpConnection* conn, const Message& message);

    EventLoop* loop_;
    std::unique_ptr<Socket> acceptor_;
    std::unique_ptr<Channel> acceptor_channel_;

    InetAddress listen_addr_;

    /// 连接名 → TcpConnection 指针 的映射。
    std::unordered_map<std::string, std::unique_ptr<TcpConnection>> connections_;

    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
    ConnectionCallback close_callback_;

    Dispatcher* dispatcher_{nullptr};
    ToolManager* tool_manager_{nullptr};

    std::shared_ptr<ThreadPool> thread_pool_;
    bool started_{false};

    static int next_conn_id_;
};

} // namespace mcp

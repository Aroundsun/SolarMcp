#pragma once

#include "mcp/common/noncopyable.h"
#include "mcp/network/inet_address.h"
#include "mcp/protocol/message.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mcp {

class EventLoop;
class TcpConnection;
class Channel;
class Socket;
class ThreadPool;
class EventLoopThreadPool;
class Dispatcher;
class ToolManager;

/// 基于 Reactor 模式的高级 TCP 服务器。
///
/// Main Reactor（base loop）负责 accept；新连接 Round-Robin 分配到
/// Worker Reactor。重业务（tools/call、plugins/reload）投递到 ThreadPool。
class TcpServer : public NonCopyable {
public:
    using MessageCallback = std::function<void(
        TcpConnection* conn, const Message& message)>;

    using ConnectionCallback = std::function<void(TcpConnection* conn)>;

    TcpServer(EventLoop* loop, const InetAddress& listen_addr);
    ~TcpServer();

    void start();
    void stop();

    void setMessageCallback(MessageCallback cb) {
        message_callback_ = std::move(cb);
    }

    void setConnectionCallback(ConnectionCallback cb) {
        connection_callback_ = std::move(cb);
    }

    void setCloseCallback(ConnectionCallback cb) {
        close_callback_ = std::move(cb);
    }

    void setThreadPool(std::shared_ptr<ThreadPool> pool) {
        thread_pool_ = std::move(pool);
    }

    void setEventLoopThreadPool(EventLoopThreadPool* pool) {
        worker_pool_ = pool;
    }

    void setMaxQueueSize(size_t max_size) {
        max_queue_size_ = max_size;
    }

    void setDispatcher(Dispatcher* dispatcher) {
        dispatcher_ = dispatcher;
    }

    void setToolManager(ToolManager* tool_manager) {
        tool_manager_ = tool_manager;
    }

    EventLoop* loop() const noexcept { return loop_; }
    Dispatcher* dispatcher() const noexcept { return dispatcher_; }
    ToolManager* toolManager() const noexcept { return tool_manager_; }

    void setAuthToken(std::string token) {
        auth_token_ = std::move(token);
        auth_enabled_ = !auth_token_.empty();
    }

    bool isAuthEnabled() const noexcept { return auth_enabled_; }

private:
    void onNewConnection(int sock_fd, const InetAddress& peer_addr);
    void createConnectionOnLoop(EventLoop* io_loop,
                                int sock_fd,
                                const InetAddress& peer_addr);
    void removeConnection(const std::shared_ptr<TcpConnection>& conn);
    void onMessage(const std::shared_ptr<TcpConnection>& conn,
                   const Message& message);
    void dispatchMessage(const std::shared_ptr<TcpConnection>& conn,
                         const Request& request);
    void handleAuthenticate(const std::shared_ptr<TcpConnection>& conn,
                            const Request& request);

    EventLoop*  loop_; // main loop
    std::unique_ptr<Socket> acceptor_;
    std::unique_ptr<Channel> acceptor_channel_;

    InetAddress listen_addr_;

    std::unordered_map<std::string, std::shared_ptr<TcpConnection>> connections_;
    mutable std::mutex connections_mutex_;

    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
    ConnectionCallback close_callback_;

    Dispatcher* dispatcher_{nullptr};
    ToolManager* tool_manager_{nullptr};

    EventLoopThreadPool* worker_pool_{nullptr};
    std::shared_ptr<ThreadPool> thread_pool_;
    size_t max_queue_size_{10000};

    bool started_{false};

    std::string auth_token_;
    bool auth_enabled_{false};

    static int next_conn_id_;
};

} // namespace mcp

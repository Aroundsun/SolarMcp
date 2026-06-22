#include "mcp/server/tcp_server.h"
#include "mcp/server/tcp_connection.h"
#include "mcp/server/dispatcher.h"
#include "mcp/reactor/event_loop.h"
#include "mcp/reactor/channel.h"
#include "mcp/network/socket.h"
#include "mcp/network/inet_address.h"
#include "mcp/protocol/json_rpc_codec.h"
#include "mcp/protocol/request.h"
#include "mcp/protocol/response.h"
#include "mcp/common/macros.h"
#include "mcp/logger/logger.h"

#include <cerrno>
#include <cstring>
#include <sstream>

namespace mcp {

int TcpServer::next_conn_id_ = 0;

// ---------------------------------------------------------------------------
// 构造
// ---------------------------------------------------------------------------

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listen_addr)
    : loop_(loop)
    , acceptor_(std::make_unique<Socket>())
    , listen_addr_(listen_addr)
{
    acceptor_->createTcp();
    acceptor_->setReuseAddr(true);
    acceptor_->bind(listen_addr_);
    acceptor_->listen();

    acceptor_channel_ = std::make_unique<Channel>(loop, acceptor_->fd());
    acceptor_channel_->setReadCallback([this] {
        // ET 模式：循环 accept 直到 EAGAIN，排空所有待处理连接
        while (true) {
            InetAddress peer_addr;
            int conn_fd = acceptor_->accept(&peer_addr);
            if (conn_fd >= 0) {
                onNewConnection(conn_fd, peer_addr);
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    break;
                }
                // 致命 accept 错误 — 记录并停止
                LOG_ERROR("TcpServer: accept error: {}", std::strerror(errno));
                break;
            }
        }
    });
}

TcpServer::~TcpServer() {
    stop();
}

// ---------------------------------------------------------------------------
// start / stop
// ---------------------------------------------------------------------------

void TcpServer::start() {
    if (started_) return;
    started_ = true;
    acceptor_channel_->enableReading();

    LOG_INFO("TcpServer listening on {}", listen_addr_.toIpPort());
}

void TcpServer::stop() {
    if (!started_) return;

    acceptor_channel_->disableAll();
    acceptor_channel_->remove();

    // 关闭所有现有连接
    std::vector<std::string> names;
    names.reserve(connections_.size());
    for (auto& kv : connections_) {
        names.push_back(kv.first);
    }
    for (const auto& name : names) {
        auto it = connections_.find(name);
        if (it != connections_.end()) {
            it->second->shutdown();
        }
    }

    started_ = false;
}

// ---------------------------------------------------------------------------
// onNewConnection
// ---------------------------------------------------------------------------

void TcpServer::onNewConnection(int sock_fd, const InetAddress& peer_addr) {
    // 生成唯一连接名
    int conn_id = next_conn_id_++;
    std::ostringstream oss;
    oss << listen_addr_.toIpPort() << "#" << conn_id;
    std::string conn_name = oss.str();

    InetAddress local_addr(0, "0.0.0.0");

    auto conn = std::make_unique<TcpConnection>(
        loop_, conn_name, sock_fd, local_addr, peer_addr);

    // 设置 JSON-RPC 编解码器以进行 Content-Length 分帧
    conn->setCodec(std::make_unique<JsonRpcCodec>());

    // 设置消息回调 — 默认：通过 onMessage 分发
    // 若用户设置了自定义 message_callback_ 则使用之
    if (message_callback_) {
        conn->setMessageCallback(message_callback_);
    } else {
        conn->setMessageCallback([this](TcpConnection* c, const Message& msg) {
            onMessage(c, msg);
        });
    }

    conn->setCloseCallback([this](TcpConnection* c) {
        if (close_callback_) {
            close_callback_(c);
        }
        removeConnection(c);
    });

    connections_[conn_name] = std::move(conn);

    LOG_INFO("TcpServer: new connection {} from {}",
             conn_name, peer_addr.toIpPort());

    // 建立连接（启用读）
    auto* conn_ptr = connections_[conn_name].get();
    loop_->runInLoop([conn_ptr] {
        conn_ptr->connectEstablished();
    });

    if (connection_callback_) {
        connection_callback_(conn_ptr);
    }
}

// ---------------------------------------------------------------------------
// removeConnection
// ---------------------------------------------------------------------------

void TcpServer::removeConnection(TcpConnection* conn) {
    std::string name = conn->name();
    // 延迟到当前事件处理结束后再销毁，避免在 Channel::handleEvent() 回调栈内 use-after-free
    loop_->queueInLoop([this, name = std::move(name)]() {
        auto it = connections_.find(name);
        if (it != connections_.end()) {
            connections_.erase(it);
        }
    });
}

// ---------------------------------------------------------------------------
// onMessage — 默认分发：Request → Dispatcher → Response → send
// ---------------------------------------------------------------------------

void TcpServer::onMessage(TcpConnection* conn, const Message& message) {
    // 仅处理 Request（服务器端不处理 Response）
    if (!isRequest(message)) {
        return;
    }

    const auto& request = std::get<Request>(message);

    LOG_DEBUG("TcpServer::onMessage: method={}, id={}",
              request.method, request.idString());

    Response response;

    if (dispatcher_ && dispatcher_->hasMethod(request.method)) {
        // 通过已注册处理函数分发
        response = dispatcher_->dispatch(request);
    } else {
        // 方法未找到
        response = Response::makeError(
            request.id,
            ErrorCodes::kMethodNotFound,
            "Method not found: " + request.method);
    }

    // 将响应发回客户端
    conn->send(Message(std::move(response)));
}

} // namespace mcp

#include "mcp/server/tcp_server.h"
#include "mcp/server/tcp_connection.h"
#include "mcp/server/dispatcher.h"
#include "mcp/reactor/event_loop.h"
#include "mcp/reactor/event_loop_thread_pool.h"
#include "mcp/reactor/channel.h"
#include "mcp/thread/thread_pool.h"
#include "mcp/network/socket.h"
#include "mcp/network/inet_address.h"
#include "mcp/protocol/json_rpc_codec.h"
#include "mcp/protocol/request.h"
#include "mcp/protocol/response.h"
#include "mcp/common/macros.h"
#include "mcp/logger/logger.h"

#include <cerrno>
#include <cstring>
#include <mutex>
#include <sstream>

namespace mcp {

int TcpServer::next_conn_id_ = 0;

namespace {

bool isAsyncMethod(const std::string& method) {
    return method == "tools/call" || method == "plugins/reload";
}

} // namespace

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
        while (true) {
            InetAddress peer_addr;
            int conn_fd = acceptor_->accept(&peer_addr);
            if (conn_fd >= 0) {
                onNewConnection(conn_fd, peer_addr);
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    break;
                }
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

    std::vector<std::shared_ptr<TcpConnection>> conns;
    {
        std::lock_guard lock(connections_mutex_);
        conns.reserve(connections_.size());
        for (auto& kv : connections_) {
            conns.push_back(kv.second);
        }
    }

    for (auto& conn : conns) {
        EventLoop* io_loop = conn->loop();
        io_loop->runInLoop([conn]() {
            conn->shutdown();
        });
    }

    started_ = false;
}

// ---------------------------------------------------------------------------
// onNewConnection
// ---------------------------------------------------------------------------

void TcpServer::onNewConnection(int sock_fd, const InetAddress& peer_addr) {
    EventLoop* io_loop = worker_pool_ ? worker_pool_->getNextLoop() : loop_;

    auto setup = [this, sock_fd, peer_addr, io_loop]() {
        createConnectionOnLoop(io_loop, sock_fd, peer_addr);
    };

    if (io_loop == loop_ && loop_->isInLoopThread()) {
        setup();
    } else {
        io_loop->runInLoop(std::move(setup));
    }
}

void TcpServer::createConnectionOnLoop(EventLoop* io_loop,
                                       int sock_fd,
                                       const InetAddress& peer_addr) {
    int conn_id = next_conn_id_++;
    std::ostringstream oss;
    oss << listen_addr_.toIpPort() << "#" << conn_id;
    std::string conn_name = oss.str();

    InetAddress local_addr(0, "0.0.0.0");

    auto conn = std::make_shared<TcpConnection>(
        io_loop, conn_name, sock_fd, local_addr, peer_addr);

    conn->setCodec(std::make_unique<JsonRpcCodec>());

    conn->setMessageCallback([this](const std::shared_ptr<TcpConnection>& c,
                                    const Message& msg) {
        onMessage(c, msg);
    });

    conn->setCloseCallback([this](const std::shared_ptr<TcpConnection>& c) {
        if (close_callback_) {
            close_callback_(c.get());
        }
        removeConnection(c);
    });

    {
        std::lock_guard lock(connections_mutex_);
        connections_[conn_name] = conn;
    }

    LOG_INFO("TcpServer: new connection {} from {} on loop {}",
             conn_name, peer_addr.toIpPort(),
             io_loop == loop_ ? "main" : "worker");

    conn->connectEstablished();

    if (connection_callback_) {
        connection_callback_(conn.get());
    }
}

// ---------------------------------------------------------------------------
// removeConnection
// ---------------------------------------------------------------------------

void TcpServer::removeConnection(const std::shared_ptr<TcpConnection>& conn) {
    std::string name = conn->name();
    EventLoop* io_loop = conn->loop();
    io_loop->queueInLoop([this, name = std::move(name)]() {
        std::lock_guard lock(connections_mutex_);
        connections_.erase(name);
    });
}

// ---------------------------------------------------------------------------
// onMessage
// ---------------------------------------------------------------------------

void TcpServer::onMessage(const std::shared_ptr<TcpConnection>& conn,
                          const Message& message) {
    if (!isRequest(message)) {
        return;
    }

    const auto& request = std::get<Request>(message);

    LOG_DEBUG("TcpServer::onMessage: method={}, id={}",
              request.method, request.idString());

    if (request.method == "authenticate") {
        handleAuthenticate(conn, request);
        return;
    }

    if (auth_enabled_ && !conn->isAuthenticated()) {
        auto response = Response::makeError(
            request.id,
            ErrorCodes::kAuthRequired,
            "Authentication required — call 'authenticate' first");
        conn->send(Message(std::move(response)));
        return;
    }

    if (message_callback_) {
        message_callback_(conn.get(), message);
        return;
    }

    dispatchMessage(conn, request);
}

// ---------------------------------------------------------------------------
// dispatchMessage
// ---------------------------------------------------------------------------

void TcpServer::dispatchMessage(const std::shared_ptr<TcpConnection>& conn,
                                const Request& request) {
    if (isAsyncMethod(request.method) && thread_pool_) {
        if (thread_pool_->queueSize() >= max_queue_size_) {
            auto response = Response::makeError(
                request.id,
                ErrorCodes::kServerBusy,
                "Server busy — task queue full");
            conn->sendInLoop(Message(std::move(response)));
            return;
        }

        std::weak_ptr<TcpConnection> weak_conn = conn;
        thread_pool_->enqueue([this, weak_conn, request]() {
            Response response;
            if (dispatcher_ && dispatcher_->hasMethod(request.method)) {
                response = dispatcher_->dispatch(request);
            } else {
                response = Response::makeError(
                    request.id,
                    ErrorCodes::kMethodNotFound,
                    "Method not found: " + request.method);
            }

            if (auto locked = weak_conn.lock()) {
                locked->sendInLoop(Message(std::move(response)));
            }
        });
        return;
    }

    Response response;
    if (dispatcher_ && dispatcher_->hasMethod(request.method)) {
        response = dispatcher_->dispatch(request);
    } else {
        response = Response::makeError(
            request.id,
            ErrorCodes::kMethodNotFound,
            "Method not found: " + request.method);
    }

    conn->send(Message(std::move(response)));
}

// ---------------------------------------------------------------------------
// handleAuthenticate
// ---------------------------------------------------------------------------

void TcpServer::handleAuthenticate(const std::shared_ptr<TcpConnection>& conn,
                                   const Request& request) {
    if (!auth_enabled_) {
        auto resp = Response::success(
            request.id,
            {{"status", "auth_disabled"}});
        conn->send(Message(std::move(resp)));
        return;
    }

    if (conn->isAuthenticated()) {
        auto resp = Response::success(
            request.id,
            {{"status", "already_authenticated"}});
        conn->send(Message(std::move(resp)));
        return;
    }

    if (!request.params.contains("token") || !request.params["token"].is_string()) {
        auto resp = Response::makeError(
            request.id,
            ErrorCodes::kInvalidParams,
            "Missing or invalid 'token' parameter");
        conn->send(Message(std::move(resp)));
        return;
    }

    std::string provided = request.params["token"].get<std::string>();
    if (provided != auth_token_) {
        LOG_WARN("TcpServer: authentication failed for connection {}",
                 conn->name());
        auto resp = Response::makeError(
            request.id,
            ErrorCodes::kAuthFailed,
            "Invalid token");
        conn->send(Message(std::move(resp)));
        return;
    }

    conn->setAuthenticated(true);
    LOG_INFO("TcpServer: connection {} authenticated", conn->name());

    auto resp = Response::success(
        request.id,
        {{"status", "authenticated"}});
    conn->send(Message(std::move(resp)));
}

} // namespace mcp

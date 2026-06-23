#include "mcp/server/tcp_connection.h"
#include "mcp/reactor/event_loop.h"
#include "mcp/reactor/channel.h"
#include "mcp/network/socket.h"
#include "mcp/network/inet_address.h"
#include "mcp/protocol/codec.h"
#include "mcp/common/macros.h"
#include "mcp/logger/logger.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace mcp {

// ---------------------------------------------------------------------------
// 构造
// ---------------------------------------------------------------------------

TcpConnection::TcpConnection(EventLoop* loop,
                             const std::string& name,
                             int sock_fd,
                             const InetAddress& local_addr,
                             const InetAddress& peer_addr)
    : loop_(loop)
    , name_(name)
    , socket_(std::make_unique<Socket>(sock_fd))
    , channel_(std::make_unique<Channel>(loop, sock_fd))
    , local_addr_(local_addr)
    , peer_addr_(peer_addr)
{
    channel_->setReadCallback([this] { handleRead(); });
    channel_->setWriteCallback([this] { handleWrite(); });
    channel_->setCloseCallback([this] { handleClose(); });
    channel_->setErrorCallback([this] { handleError(); });
}

TcpConnection::~TcpConnection() {
    // 析构前须移除 Channel
}

// ---------------------------------------------------------------------------
// connectEstablished
// ---------------------------------------------------------------------------

void TcpConnection::connectEstablished() {
    state_ = State::kConnected;
    channel_->enableReading();
}

// ---------------------------------------------------------------------------
// shutdown
// ---------------------------------------------------------------------------

void TcpConnection::shutdown() {
    if (state_ == State::kConnected) {
        state_ = State::kDisconnecting;
        socket_->shutdownWrite();
        channel_->disableAll();
        // 给客户端时间读取剩余数据
        loop_->runInLoop([this] {
            handleClose();
        });
    }
}

// ---------------------------------------------------------------------------
// send — Message 变体（由 codec 编码）
// ---------------------------------------------------------------------------

void TcpConnection::send(const Message& message) {
    if (!codec_) {
        LOG_ERROR("TcpConnection::send: no codec set");
        return;
    }
    std::string encoded = codec_->encode(message);
    send(encoded);
}

void TcpConnection::send(const std::string& data) {
    send(data.data(), data.size());
}

void TcpConnection::send(const char* data, size_t len) {
    if (state_ != State::kConnected) {
        return;
    }

    if (output_buffer_.readableBytes() == 0) {
        ssize_t n = ::write(channel_->fd(), data, len);
        if (n >= 0) {
            if (static_cast<size_t>(n) == len) {
                return;
            }
            data += n;
            len -= n;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            handleClose();
            return;
        }
    }

    output_buffer_.append(data, len);
    channel_->enableWriting();
}

void TcpConnection::sendInLoop(const Message& message) {
    if (loop_->isInLoopThread()) {
        send(message);
        return;
    }

    loop_->queueInLoop([self = shared_from_this(), message]() {
        self->send(message);
    });
}

// ---------------------------------------------------------------------------
// setCodec
// ---------------------------------------------------------------------------

void TcpConnection::setCodec(std::unique_ptr<Codec> codec) {
    codec_ = std::move(codec);
}

// ---------------------------------------------------------------------------
// handleRead — EPOLLIN 回调
// ---------------------------------------------------------------------------

void TcpConnection::handleRead() {
    // 循环读取直到 EAGAIN（边缘触发模式要求）
    char buf[65536];
    while (true) {
        ssize_t n = ::read(channel_->fd(), buf, sizeof(buf));
        if (n > 0) {
            input_buffer_.append(buf, static_cast<size_t>(n));
        } else if (n == 0) {
            // 对端关闭连接
            handleClose();
            return;
        } else {
            int err = errno;
            if (err == EAGAIN || err == EWOULDBLOCK) {
                break; // 无更多数据
            } else if (err == EINTR) {
                continue; // 被中断，重试
            } else {
                LOG_ERROR("TcpConnection::handleRead error: {}",
                          std::strerror(err));
                handleClose();
                return;
            }
        }
    }

    processInput();
}

// ---------------------------------------------------------------------------
// handleWrite — EPOLLOUT 回调
// ---------------------------------------------------------------------------

void TcpConnection::handleWrite() {
    if (output_buffer_.readableBytes() == 0) {
        channel_->disableWriting();
        return;
    }

    ssize_t n = ::write(channel_->fd(),
                        output_buffer_.peek(),
                        output_buffer_.readableBytes());
    if (n > 0) {
        output_buffer_.retrieve(static_cast<size_t>(n));

        if (output_buffer_.readableBytes() == 0) {
            channel_->disableWriting();
        }
    } else {
        int err = errno;
        if (err != EAGAIN && err != EWOULDBLOCK && err != EINTR) {
            LOG_ERROR("TcpConnection::handleWrite error: {}",
                      std::strerror(err));
            handleClose();
        }
    }
}

// ---------------------------------------------------------------------------
// handleClose
// ---------------------------------------------------------------------------

void TcpConnection::handleClose() {
    if (state_ == State::kDisconnected) {
        return;
    }
    state_ = State::kDisconnected;

    channel_->disableAll();
    channel_->remove();

    LOG_INFO("TcpConnection {} closed", name_);

    if (close_callback_) {
        close_callback_(shared_from_this());
    }
}

// ---------------------------------------------------------------------------
// handleError
// ---------------------------------------------------------------------------

void TcpConnection::handleError() {
    LOG_ERROR("TcpConnection {} error", name_);
    handleClose();
}

// ---------------------------------------------------------------------------
// processInput — 通过 Codec 的 Content-Length 消息分帧
// ---------------------------------------------------------------------------

void TcpConnection::processInput() {
    if (!codec_) {
        LOG_ERROR("TcpConnection::processInput: no codec set");
        return;
    }

    // 只要 codec 能提取消息就继续解码
    while (true) {
        auto maybe_msg = codec_->decode(input_buffer_);
        if (!maybe_msg.has_value()) {
            break; // 完整消息需要更多数据
        }

        if (message_callback_) {
            message_callback_(shared_from_this(), maybe_msg.value());
        }
    }
}

} // namespace mcp

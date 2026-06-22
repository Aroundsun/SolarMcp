#include "mcp/network/socket.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace mcp {

// ---------------------------------------------------------------------------
// 构造 / 析构
// ---------------------------------------------------------------------------

Socket::Socket() : fd_(-1) {}

Socket::Socket(int fd) noexcept : fd_(fd) {}

Socket::~Socket() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// createTcp
// ---------------------------------------------------------------------------

void Socket::createTcp() {
    fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd_ < 0) {
        // 旧内核回退方案
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ >= 0) {
            setNonBlock(true);
        }
    }
}

// ---------------------------------------------------------------------------
// bind
// ---------------------------------------------------------------------------

void Socket::bind(const InetAddress& addr) {
    int ret = ::bind(fd_, addr.sockAddr(), addr.sockLen());
    if (ret < 0) {
        throw std::runtime_error(
            "Socket::bind failed on " + addr.toIpPort() + ": " +
            std::strerror(errno));
    }
}

// ---------------------------------------------------------------------------
// listen
// ---------------------------------------------------------------------------

void Socket::listen(int backlog) {
    int ret = ::listen(fd_, backlog);
    if (ret < 0) {
        throw std::runtime_error("Socket::listen failed");
    }
}

// ---------------------------------------------------------------------------
// accept
// ---------------------------------------------------------------------------

int Socket::accept(InetAddress* peer_addr) {
    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    int conn_fd = ::accept4(fd_,
                            reinterpret_cast<sockaddr*>(&addr),
                            &addr_len,
                            SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (conn_fd >= 0 && peer_addr) {
        *peer_addr = InetAddress(addr);
    }
    return conn_fd;
}

// ---------------------------------------------------------------------------
// setReuseAddr
// ---------------------------------------------------------------------------

void Socket::setReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval,
                 static_cast<socklen_t>(sizeof(optval)));
}

// ---------------------------------------------------------------------------
// setNonBlock
// ---------------------------------------------------------------------------

void Socket::setNonBlock(bool on) {
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags < 0) return;
    if (on) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    ::fcntl(fd_, F_SETFL, flags);
}

// ---------------------------------------------------------------------------
// shutdownWrite
// ---------------------------------------------------------------------------

void Socket::shutdownWrite() {
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_WR);
    }
}

} // namespace mcp

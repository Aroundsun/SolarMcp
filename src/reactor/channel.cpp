#include "mcp/reactor/channel.h"
#include "mcp/reactor/event_loop.h"

#include <sys/epoll.h>
#include <unistd.h>

namespace mcp {

// ---------------------------------------------------------------------------
// 构造 / 析构
// ---------------------------------------------------------------------------

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop)
    , fd_(fd)
{}

Channel::~Channel() {
    // 所有者（TcpConnection）须在析构前调用 disableAll()
}

// ---------------------------------------------------------------------------
// handleEvent
// ---------------------------------------------------------------------------

void Channel::handleEvent() {
    // 先检查错误或挂断
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (close_callback_) {
            close_callback_();
        }
        return;
    }

    if (revents_ & (EPOLLERR)) {
        if (error_callback_) {
            error_callback_();
        }
        return;
    }

    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (read_callback_) {
            read_callback_();
        }
    }

    if (revents_ & EPOLLOUT) {
        if (write_callback_) {
            write_callback_();
        }
    }
}

// ---------------------------------------------------------------------------
// enableReading / enableWriting / disableReading / disableWriting / disableAll
// ---------------------------------------------------------------------------

void Channel::enableReading() {
    events_ |= kReadEvent | EPOLLET;
    update();
}

void Channel::enableWriting() {
    events_ |= kWriteEvent | EPOLLET;
    update();
}

void Channel::disableReading() {
    events_ &= ~static_cast<uint32_t>(kReadEvent);
    update();
}

void Channel::disableWriting() {
    events_ &= ~static_cast<uint32_t>(kWriteEvent);
    update();
}

void Channel::disableAll() {
    events_ = kNoneEvent;
    update();
}

// ---------------------------------------------------------------------------
// remove
// ---------------------------------------------------------------------------

void Channel::remove() {
    loop_->removeChannel(this);
}

// ---------------------------------------------------------------------------
// update — 通知 EventLoop 关注的事件已变更
// ---------------------------------------------------------------------------

void Channel::update() {
    loop_->updateChannel(this);
}

} // namespace mcp

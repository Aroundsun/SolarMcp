#include "mcp/reactor/event_loop.h"
#include "mcp/reactor/channel.h"
#include "mcp/reactor/epoll_poller.h"
#include "mcp/timer/timer_wheel.h"

#include <sys/eventfd.h>
#include <unistd.h>

namespace mcp {

// ---------------------------------------------------------------------------
// 构造
// ---------------------------------------------------------------------------

EventLoop::EventLoop()
    : poller_(std::make_unique<EpollPoller>())
{
    wakeup_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wakeup_fd_ < 0) {
        throw std::runtime_error("EventLoop: eventfd creation failed");
    }
    wakeup_channel_ = std::make_unique<Channel>(this, wakeup_fd_);
    wakeup_channel_->setReadCallback([this] { handleWakeup(); });
    wakeup_channel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeup_channel_->disableAll();
    wakeup_channel_->remove();
    if (wakeup_fd_ >= 0) {
        ::close(wakeup_fd_);
        wakeup_fd_ = -1;
    }
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------

void EventLoop::loop() {
    looping_.store(true, std::memory_order_release);
    quit_.store(false, std::memory_order_release);
    thread_id_ = std::this_thread::get_id();

    while (!quit_.load(std::memory_order_acquire)) {
        active_channels_.clear();
        poller_->poll(10000, active_channels_);

        // 分发事件
        for (auto* channel : active_channels_) {
            channel->handleEvent();
        }

        // 若已附加则 tick 时间轮 — poll_ms 提供近似
        // 当前时间以限制 tick 粒度。
        if (timer_wheel_) {
            timer_wheel_->tick();
        }

        // 执行待处理 functor
        doPendingFunctors();
    }

    looping_.store(false, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// quit
// ---------------------------------------------------------------------------

void EventLoop::quit() {
    quit_.store(true, std::memory_order_release);
    if (!isInLoopThread()) {
        wakeup();
    }
}

// ---------------------------------------------------------------------------
// updateChannel / removeChannel / hasChannel
// ---------------------------------------------------------------------------

void EventLoop::updateChannel(Channel* channel) {
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel) const {
    // 隐式委托给 poller — index >= 0 的 channel 被跟踪
    return channel->index() >= 0;
}

// ---------------------------------------------------------------------------
// runInLoop / queueInLoop
// ---------------------------------------------------------------------------

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}
// 将 functor 加入队列并在必要时唤醒 loop。
// 若从 loop 线程自身调用则跳过 wakeup()：functor 会在当前迭代
// 末尾的 doPendingFunctors() 中被取出执行，无需通过 eventfd 唤醒。
// 若从其他线程调用，必须 wakeup() 以打断 epoll_wait。

void EventLoop::queueInLoop(Functor cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_functors_.push_back(std::move(cb));
    }

    if (!isInLoopThread()) {
        wakeup();
    }
}

// ---------------------------------------------------------------------------
// doPendingFunctors
// ---------------------------------------------------------------------------

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pending_functors_);
    }
    for (auto& f : functors) {
        f();
    }
}

// ---------------------------------------------------------------------------
// wakeup / handleWakeup
// ---------------------------------------------------------------------------

void EventLoop::wakeup() {
    uint64_t one = 1;
    ::write(wakeup_fd_, &one, sizeof(one));
}

void EventLoop::handleWakeup() {
    uint64_t one;
    ::read(wakeup_fd_, &one, sizeof(one));
}

} // namespace mcp

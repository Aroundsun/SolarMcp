#include "mcp/reactor/event_loop_thread.h"
#include "mcp/reactor/event_loop.h"

namespace mcp {

EventLoopThread::EventLoopThread() = default;

EventLoopThread::~EventLoopThread() {
    stop();
}

EventLoop* EventLoopThread::startLoop() {
    thread_ = std::thread([this] {
        EventLoop loop;
        {
            std::lock_guard lock(mutex_);
            loop_ = &loop;
            cond_.notify_one();
        }
        loop.loop();
        {
            std::lock_guard lock(mutex_);
            loop_ = nullptr;
        }
    });

    std::unique_lock lock(mutex_);
    while (loop_ == nullptr) {
        cond_.wait(lock);
    }
    return loop_;
}

void EventLoopThread::stop() {
    EventLoop* loop = nullptr;
    {
        std::lock_guard lock(mutex_);
        loop = loop_;
    }
    if (loop != nullptr) {
        loop->quit();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

EventLoop* EventLoopThread::getLoop() const {
    std::lock_guard lock(mutex_);
    return loop_;
}

} // namespace mcp

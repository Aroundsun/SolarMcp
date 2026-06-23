#include "mcp/reactor/event_loop_thread_pool.h"
#include "mcp/reactor/event_loop_thread.h"
#include "mcp/reactor/event_loop.h"

namespace mcp {

EventLoopThreadPool::EventLoopThreadPool(EventLoop* base_loop)
    : base_loop_(base_loop) {}

EventLoopThreadPool::~EventLoopThreadPool() {
    stop();
}

void EventLoopThreadPool::setThreadNum(int num_threads) {
    if (started_) {
        return;
    }
    num_threads_ = num_threads < 0 ? 0 : num_threads;
}

void EventLoopThreadPool::start() {
    if (started_) {
        return;
    }
    started_ = true;

    for (int i = 0; i < num_threads_; ++i) {
        auto thread = std::make_unique<EventLoopThread>();
        EventLoop* loop = thread->startLoop();
        loops_.push_back(loop);
        threads_.push_back(std::move(thread));
    }
}

void EventLoopThreadPool::stop() {
    if (!started_) {
        return;
    }

    for (auto& thread : threads_) {
        thread->stop();
    }
    threads_.clear();
    loops_.clear();
    next_ = 0;
    started_ = false;
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    if (loops_.empty()) {
        return base_loop_;
    }

    std::lock_guard lock(mutex_);
    EventLoop* loop = loops_[static_cast<size_t>(next_)];
    next_ = (next_ + 1) % static_cast<int>(loops_.size());
    return loop;
}

} // namespace mcp

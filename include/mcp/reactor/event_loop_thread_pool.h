#pragma once

#include "mcp/common/noncopyable.h"

#include <memory>
#include <mutex>
#include <vector>

namespace mcp {

class EventLoop;
class EventLoopThread;

/// Worker Reactor 池：Round-Robin 将新连接分配到各 IO 线程。
///
/// num_threads == 0 时 getNextLoop() 始终返回 base_loop（单 Reactor 模式）。
class EventLoopThreadPool : public NonCopyable {
public:
    explicit EventLoopThreadPool(EventLoop* base_loop);
    ~EventLoopThreadPool();

    void setThreadNum(int num_threads);
    void start();
    void stop();

    /// 返回下一个 Worker EventLoop；无 Worker 时返回 base_loop。
    EventLoop* getNextLoop();

    EventLoop* baseLoop() const noexcept { return base_loop_; }

    size_t size() const noexcept { return loops_.size(); }
    bool started() const noexcept { return started_; }

private:
    EventLoop* base_loop_;
    bool started_{false};
    int num_threads_{0};
    int next_{0};
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};

} // namespace mcp

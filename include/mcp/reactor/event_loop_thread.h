#pragma once

#include "mcp/common/noncopyable.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace mcp {

class EventLoop;

/// 在独立线程中运行 EventLoop（One Loop Per Thread）。
class EventLoopThread : public NonCopyable {
public:
    EventLoopThread();
    ~EventLoopThread();

    /// 启动线程并阻塞直到 EventLoop 就绪，返回 loop 指针。
    EventLoop* startLoop();

    /// 请求 loop 退出并 join 线程。
    void stop();

    EventLoop* getLoop() const;

private:
    EventLoop* loop_{nullptr};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
};

} // namespace mcp

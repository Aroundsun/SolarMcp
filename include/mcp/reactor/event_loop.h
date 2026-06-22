#pragma once

#include "mcp/common/noncopyable.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace mcp {

class Channel;
class Poller;
class TimerWheel;

/// 基于 epoll 的事件驱动 I/O 循环。
///
/// 每个 EventLoop 在独立线程中运行（One Loop Per Thread）。
/// 拥有 EpollPoller 和 Channel 集合。
///
/// 主要特性：
/// - `runInLoop()`：从任意线程安全调度回调
/// - `loop()`：主事件循环（阻塞），由 epoll_wait 驱动
/// - `quit()`：优雅退出
///
/// 线程安全：多数方法仅能在 loop 所属线程调用。
/// `runInLoop()` 例外 — 可从任意线程调用。
class EventLoop : public NonCopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    /// 进入主事件循环，阻塞直到 quit() 被调用。
    void loop();

    /// 通知 loop 在当前迭代后退出。
    void quit();

    /// 向 poller 注册或更新 channel 关注的事件。
    void updateChannel(Channel* channel);

    /// 从 poller 移除 channel。
    void removeChannel(Channel* channel);

    /// 该 channel 是否已在此 loop 中注册？
    bool hasChannel(Channel* channel) const;

    /// 调度 functor 在 loop 线程执行。
    /// 从 loop 线程调用则立即执行。
    /// 从其他线程调用则排队稍后执行。
    void runInLoop(Functor cb);

    /// 将 functor 排队在 loop 线程执行。
    void queueInLoop(Functor cb);

    /// 调用线程为 loop 所属线程时返回 true。
    bool isInLoopThread() const {
        return thread_id_ == std::this_thread::get_id();
    }

    /// 附加 TimerWheel 以周期性 tick 驱动定时器。
    /// 每次 loop 迭代使用 poll() 时间戳 tick 一次。
    /// 传入 nullptr 可分离。
    void setTimerWheel(TimerWheel* timer_wheel) {
        timer_wheel_ = timer_wheel;
    }

private:
    /// 执行所有待处理 functor。
    void doPendingFunctors();

    /// 唤醒事件循环（runInLoop 用于打断 epoll_wait）。
    void wakeup();

    /// 处理 wakeup fd 可读事件。
    void handleWakeup();

    std::unique_ptr<Poller> poller_;

    /// 当前注册到此 loop 的所有 channel。
    /// 由用户持有（TcpConnection 拥有其 Channel）。
    std::vector<Channel*> active_channels_;

    std::atomic<bool> looping_{false};
    std::atomic<bool> quit_{false};
    std::thread::id thread_id_;

    /// 待在 loop 线程执行的 functor。
    std::vector<Functor> pending_functors_;
    std::mutex mutex_;

    /// 唤醒机制：向 wakeup_fd_ 写入一字节以打断 epoll_wait。
    int wakeup_fd_{-1};
    std::unique_ptr<Channel> wakeup_channel_;

    /// 可选定时器轮，用于周期性 tick 执行。
    TimerWheel* timer_wheel_{nullptr};
};

} // namespace mcp

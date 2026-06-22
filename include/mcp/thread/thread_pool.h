#pragma once

#include "mcp/common/noncopyable.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace mcp {

/// 固定大小线程池，基于 future 提交任务。
///
/// 工作线程在共享条件变量上阻塞。任务入队时
/// 唤醒一个工作线程执行。池支持优雅关闭：
/// 排空剩余任务后再 join 所有线程。
///
/// 线程安全：所有公开方法均可从任意线程调用。
class ThreadPool : public NonCopyable {
public:
    /// 构造并启动 `num_threads` 个工作线程。
    /// 传入 0 时默认为 `std::thread::hardware_concurrency()`。
    explicit ThreadPool(size_t num_threads = 0);

    /// 关闭线程池，等待队列任务完成后再 join 所有工作线程。
    ~ThreadPool();

    /// 提交可调用对象及参数执行。
    /// 返回持有结果（或异常）的 std::future。
    ///
    /// 用法：
    ///   auto fut = pool.enqueue([](int x) { return x * x; }, 4);
    ///   int result = fut.get(); // 16
    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>>;

    /// 发起优雅关闭，工作线程完成当前任务后退出。
    void shutdown();

    /// 工作线程数量。
    size_t size() const noexcept { return workers_.size(); }

    /// 队列中任务的大致数量。
    size_t queueSize() const;

private:
    /// 每个线程执行的工作循环。
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;

    mutable std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> stop_{false};
};

// ---------------------------------------------------------------------------
// 模板实现
// ---------------------------------------------------------------------------

template <typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result_t<F, Args...>> {

    using ReturnType = typename std::invoke_result_t<F, Args...>;

    // 将任务包装为 shared_ptr，以便 future 持有结果
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<ReturnType> result = task->get_future();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_.load(std::memory_order_acquire)) {
            throw std::runtime_error("ThreadPool::enqueue on stopped pool");
        }
        tasks_.emplace([task = std::move(task)]() {
            (*task)();
        });
    }
    cond_.notify_one();
    return result;
}

} // namespace mcp

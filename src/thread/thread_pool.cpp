#include "mcp/thread/thread_pool.h"

#include <stdexcept>

namespace mcp {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ThreadPool::ThreadPool(size_t num_threads) {
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
    }
    if (num_threads == 0) {
        num_threads = 4; // fallback
    }

    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::workerLoop, this);
    }
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

ThreadPool::~ThreadPool() {
    shutdown();
}

// ---------------------------------------------------------------------------
// shutdown
// ---------------------------------------------------------------------------

void ThreadPool::shutdown() {
    stop_.store(true, std::memory_order_release);
    cond_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

// ---------------------------------------------------------------------------
// queueSize
// ---------------------------------------------------------------------------

size_t ThreadPool::queueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

// ---------------------------------------------------------------------------
// workerLoop
// ---------------------------------------------------------------------------

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait(lock, [this] {
                return stop_.load(std::memory_order_acquire) || !tasks_.empty();
            });

            if (stop_.load(std::memory_order_acquire) && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        task();
    }
}

} // namespace mcp

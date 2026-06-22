#include "mcp/thread/thread_pool.h"

#include <gtest/gtest.h>
#include <atomic>

namespace {

TEST(ThreadPoolTest, ConstructAndShutdown) {
    mcp::ThreadPool pool(4);
    EXPECT_EQ(pool.size(), 4u);
    pool.shutdown();
}

TEST(ThreadPoolTest, EnqueueAndGetResult) {
    mcp::ThreadPool pool(2);

    auto fut1 = pool.enqueue([](int x) { return x * x; }, 5);
    auto fut2 = pool.enqueue([](int a, int b) { return a + b; }, 3, 7);

    EXPECT_EQ(fut1.get(), 25);
    EXPECT_EQ(fut2.get(), 10);

    pool.shutdown();
}

TEST(ThreadPoolTest, EnqueueVoidTask) {
    mcp::ThreadPool pool(2);
    std::atomic<int> counter{0};

    auto fut = pool.enqueue([&counter]() {
        counter.fetch_add(1);
    });

    fut.get();
    EXPECT_EQ(counter.load(), 1);

    pool.shutdown();
}

TEST(ThreadPoolTest, MultipleTasks) {
    mcp::ThreadPool pool(4);
    const int kNumTasks = 100;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < kNumTasks; ++i) {
        futures.push_back(pool.enqueue([&counter]() {
            counter.fetch_add(1);
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    EXPECT_EQ(counter.load(), kNumTasks);
    pool.shutdown();
}

TEST(ThreadPoolTest, EnqueueAfterShutdownThrows) {
    mcp::ThreadPool pool(2);
    pool.shutdown();

    EXPECT_THROW({
        pool.enqueue([]() { return 42; });
    }, std::runtime_error);
}

TEST(ThreadPoolTest, QueueSize) {
    mcp::ThreadPool pool(1);
    EXPECT_EQ(pool.queueSize(), 0u);

    // 提交一个长时间运行的任务以阻塞工作线程
    auto fut = pool.enqueue([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return 42;
    });

    // 提交更多将进入队列的任务
    auto fut2 = pool.enqueue([]() { return 1; });
    auto fut3 = pool.enqueue([]() { return 2; });

    // 至少应有一些任务在队列中
    EXPECT_GE(pool.queueSize(), 1u);

    fut.get();
    fut2.get();
    fut3.get();
    pool.shutdown();
}

} // anonymous namespace

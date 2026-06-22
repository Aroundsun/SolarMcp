#include "mcp/reactor/event_loop.h"
#include "mcp/reactor/channel.h"
#include "mcp/reactor/epoll_poller.h"
#include "mcp/network/socket.h"
#include "mcp/network/inet_address.h"
#include "mcp/network/buffer.h"
#include "mcp/server/tcp_server.h"
#include "mcp/server/tcp_connection.h"

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <chrono>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

namespace {

TEST(ReactorTest, EventLoopStartAndQuit) {
    mcp::EventLoop loop;

    std::thread loop_thread([&]() {
        loop.runInLoop([&]() { loop.quit(); });
        loop.loop();
    });

    loop_thread.join();
}

TEST(ReactorTest, RunInLoop) {
    mcp::EventLoop loop;
    std::atomic<int> counter{0};

    std::thread loop_thread([&]() {
        loop.runInLoop([&]() { counter.fetch_add(1); });
        loop.runInLoop([&]() { counter.fetch_add(2); });
        loop.runInLoop([&]() { loop.quit(); });
        loop.loop();
    });

    loop_thread.join();
    EXPECT_EQ(counter.load(), 3);
}

TEST(ReactorTest, EchoServer) {
    const int kTestPort = 18900;

    mcp::EventLoop server_loop;
    mcp::InetAddress addr(static_cast<uint16_t>(kTestPort), "127.0.0.1");
    mcp::TcpServer server(&server_loop, addr);

    server.setMessageCallback([](mcp::TcpConnection* /*conn*/, const mcp::Message& /*msg*/) {
        // 仅连接级冒烟测试；无需 echo 处理函数。
    });

    std::atomic<bool> server_ready{false};

    std::thread server_thread([&]() {
        server.start();
        server_ready.store(true);
        server_loop.loop();
    });

    while (!server_ready.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(client_fd, 0);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(kTestPort);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    int ret = ::connect(client_fd,
                        reinterpret_cast<sockaddr*>(&server_addr),
                        sizeof(server_addr));
    ASSERT_GE(ret, 0) << "Failed to connect to echo server";

    const char* msg = "Hello\r\n";
    ssize_t n = ::write(client_fd, msg, strlen(msg));
    EXPECT_EQ(n, static_cast<ssize_t>(strlen(msg)));

    ::close(client_fd);

    server_loop.quit();
    server_thread.join();
}

} // anonymous namespace

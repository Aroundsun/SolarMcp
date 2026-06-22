#include "mcp/server/tcp_server.h"
#include "mcp/server/tcp_connection.h"
#include "mcp/server/dispatcher.h"
#include "mcp/reactor/event_loop.h"
#include "mcp/network/inet_address.h"
#include "mcp/protocol/json_rpc_codec.h"
#include "mcp/protocol/message.h"
#include "mcp/protocol/request.h"
#include "mcp/protocol/response.h"
#include "mcp/common/types.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr int kAuthTestPort = 18902;
constexpr const char* kTestToken = "test-secret-token";

// ---------------------------------------------------------------------------
// TCP + JSON-RPC 辅助函数
// ---------------------------------------------------------------------------

int connectToServer(uint16_t port = kAuthTestPort) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

std::string encodeFrame(const std::string& json_body) {
    return "Content-Length: " + std::to_string(json_body.size())
           + "\r\n\r\n" + json_body;
}

bool writeAll(int fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = ::write(fd, data.data() + sent, data.size() - sent);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

std::optional<nlohmann::json> readJsonRpcResponse(int fd) {
    std::string data;
    char buf[4096];

    while (data.find("\r\n\r\n") == std::string::npos) {
        ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n <= 0) {
            return std::nullopt;
        }
        data.append(buf, static_cast<size_t>(n));
    }

    const auto header_end = data.find("\r\n\r\n");
    const std::string header = data.substr(0, header_end);
    const std::string prefix = "Content-Length: ";
    if (header.compare(0, prefix.size(), prefix) != 0) {
        return std::nullopt;
    }

    const size_t body_len = static_cast<size_t>(
        std::stoul(header.substr(prefix.size())));
    std::string body = data.substr(header_end + 4);

    while (body.size() < body_len) {
        ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n <= 0) {
            return std::nullopt;
        }
        body.append(buf, static_cast<size_t>(n));
    }

    try {
        return nlohmann::json::parse(body.substr(0, body_len));
    } catch (const nlohmann::json::parse_error&) {
        return std::nullopt;
    }
}

std::optional<nlohmann::json> sendRequest(int fd,
                                          const std::string& method,
                                          const nlohmann::json& params,
                                          int id) {
    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params},
        {"id", id},
    };
    if (!writeAll(fd, encodeFrame(req.dump()))) {
        return std::nullopt;
    }
    return readJsonRpcResponse(fd);
}

// ---------------------------------------------------------------------------
// 测试用服务器
// ---------------------------------------------------------------------------

class AuthTestServer {
public:
    AuthTestServer() : server_(&loop_, addr_) {
        dispatcher_.registerMethod("tools/list",
            [](const nlohmann::json& /*params*/) -> nlohmann::json {
                return {{"tools", nlohmann::json::array()}};
            });
        server_.setDispatcher(&dispatcher_);
    }

    void setAuthToken(const std::string& token) {
        server_.setAuthToken(token);
    }

    void setMessageCallback(mcp::TcpServer::MessageCallback cb) {
        server_.setMessageCallback(std::move(cb));
    }

    void start() {
        thread_ = std::thread([this]() {
            server_.start();
            ready_.store(true, std::memory_order_release);
            loop_.loop();
        });

        while (!ready_.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void stop() {
        loop_.quit();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    mcp::TcpServer& server() { return server_; }

private:
    mcp::EventLoop loop_;
    mcp::InetAddress addr_{static_cast<uint16_t>(kAuthTestPort), "127.0.0.1"};
    mcp::Dispatcher dispatcher_;
    mcp::TcpServer server_;
    std::thread thread_;
    std::atomic<bool> ready_{false};
};

// ---------------------------------------------------------------------------
// 单元测试：setAuthToken / isAuthEnabled
// ---------------------------------------------------------------------------

TEST(AuthConfigTest, SetAuthTokenEnablesAndDisables) {
    mcp::EventLoop loop;
    mcp::InetAddress addr(static_cast<uint16_t>(18903), "127.0.0.1");
    mcp::TcpServer server(&loop, addr);

    EXPECT_FALSE(server.isAuthEnabled());

    server.setAuthToken(kTestToken);
    EXPECT_TRUE(server.isAuthEnabled());

    server.setAuthToken("");
    EXPECT_FALSE(server.isAuthEnabled());
}

// ---------------------------------------------------------------------------
// 集成测试：认证流程
// ---------------------------------------------------------------------------

TEST(AuthIntegrationTest, DisabledAuthAllowsDirectAccess) {
    AuthTestServer fixture;
    fixture.start();

    int fd = connectToServer();
    ASSERT_GE(fd, 0);

    auto resp = sendRequest(fd, "tools/list", nlohmann::json::object(), 1);
    ASSERT_TRUE(resp.has_value());
    EXPECT_FALSE(resp->contains("error"));
    EXPECT_TRUE(resp->contains("result"));

    ::close(fd);
    fixture.stop();
}

TEST(AuthIntegrationTest, AuthenticateWhenAuthDisabled) {
    AuthTestServer fixture;
    fixture.start();

    int fd = connectToServer();
    ASSERT_GE(fd, 0);

    auto resp = sendRequest(fd, "authenticate",
                            {{"token", "any-token"}}, 1);
    ASSERT_TRUE(resp.has_value());
    EXPECT_FALSE(resp->contains("error"));
    EXPECT_EQ((*resp)["result"]["status"], "auth_disabled");

    ::close(fd);
    fixture.stop();
}

TEST(AuthIntegrationTest, EnabledAuthBlocksUnauthenticatedRequest) {
    AuthTestServer fixture;
    fixture.setAuthToken(kTestToken);
    fixture.start();

    int fd = connectToServer();
    ASSERT_GE(fd, 0);

    auto resp = sendRequest(fd, "tools/list", nlohmann::json::object(), 1);
    ASSERT_TRUE(resp.has_value());
    ASSERT_TRUE(resp->contains("error"));
    EXPECT_EQ((*resp)["error"]["code"], mcp::ErrorCodes::kAuthRequired);

    ::close(fd);
    fixture.stop();
}

TEST(AuthIntegrationTest, AuthenticateWithValidToken) {
    AuthTestServer fixture;
    fixture.setAuthToken(kTestToken);
    fixture.start();

    int fd = connectToServer();
    ASSERT_GE(fd, 0);

    auto auth_resp = sendRequest(fd, "authenticate",
                                 {{"token", kTestToken}}, 1);
    ASSERT_TRUE(auth_resp.has_value());
    EXPECT_FALSE(auth_resp->contains("error"));
    EXPECT_EQ((*auth_resp)["result"]["status"], "authenticated");

    auto list_resp = sendRequest(fd, "tools/list", nlohmann::json::object(), 2);
    ASSERT_TRUE(list_resp.has_value());
    EXPECT_FALSE(list_resp->contains("error"));
    EXPECT_TRUE(list_resp->contains("result"));

    ::close(fd);
    fixture.stop();
}

TEST(AuthIntegrationTest, AuthenticateWithInvalidToken) {
    AuthTestServer fixture;
    fixture.setAuthToken(kTestToken);
    fixture.start();

    int fd = connectToServer();
    ASSERT_GE(fd, 0);

    auto resp = sendRequest(fd, "authenticate",
                            {{"token", "wrong-token"}}, 1);
    ASSERT_TRUE(resp.has_value());
    ASSERT_TRUE(resp->contains("error"));
    EXPECT_EQ((*resp)["error"]["code"], mcp::ErrorCodes::kAuthFailed);

    ::close(fd);
    fixture.stop();
}

TEST(AuthIntegrationTest, AuthenticateMissingTokenParam) {
    AuthTestServer fixture;
    fixture.setAuthToken(kTestToken);
    fixture.start();

    int fd = connectToServer();
    ASSERT_GE(fd, 0);

    auto resp = sendRequest(fd, "authenticate", nlohmann::json::object(), 1);
    ASSERT_TRUE(resp.has_value());
    ASSERT_TRUE(resp->contains("error"));
    EXPECT_EQ((*resp)["error"]["code"], mcp::ErrorCodes::kInvalidParams);

    ::close(fd);
    fixture.stop();
}

TEST(AuthIntegrationTest, AlreadyAuthenticated) {
    AuthTestServer fixture;
    fixture.setAuthToken(kTestToken);
    fixture.start();

    int fd = connectToServer();
    ASSERT_GE(fd, 0);

    auto first = sendRequest(fd, "authenticate", {{"token", kTestToken}}, 1);
    ASSERT_TRUE(first.has_value());
    EXPECT_FALSE(first->contains("error"));

    auto second = sendRequest(fd, "authenticate", {{"token", kTestToken}}, 2);
    ASSERT_TRUE(second.has_value());
    EXPECT_FALSE(second->contains("error"));
    EXPECT_EQ((*second)["result"]["status"], "already_authenticated");

    ::close(fd);
    fixture.stop();
}

TEST(AuthIntegrationTest, NewConnectionRequiresReauth) {
    AuthTestServer fixture;
    fixture.setAuthToken(kTestToken);
    fixture.start();

    int fd1 = connectToServer();
    ASSERT_GE(fd1, 0);
    auto auth = sendRequest(fd1, "authenticate", {{"token", kTestToken}}, 1);
    ASSERT_TRUE(auth.has_value());
    EXPECT_FALSE(auth->contains("error"));
    ::close(fd1);

    int fd2 = connectToServer();
    ASSERT_GE(fd2, 0);
    auto blocked = sendRequest(fd2, "tools/list", nlohmann::json::object(), 2);
    ASSERT_TRUE(blocked.has_value());
    EXPECT_EQ((*blocked)["error"]["code"], mcp::ErrorCodes::kAuthRequired);
    ::close(fd2);

    fixture.stop();
}

TEST(AuthIntegrationTest, CustomCallbackBlockedWithoutAuth) {
    std::atomic<int> callback_count{0};

    AuthTestServer fixture;
    fixture.setAuthToken(kTestToken);
    fixture.setMessageCallback([&](mcp::TcpConnection* /*conn*/,
                                   const mcp::Message& msg) {
        if (mcp::isRequest(msg)) {
            callback_count.fetch_add(1);
        }
    });
    fixture.start();

    int fd = connectToServer();
    ASSERT_GE(fd, 0);

    auto resp = sendRequest(fd, "tools/list", nlohmann::json::object(), 1);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ((*resp)["error"]["code"], mcp::ErrorCodes::kAuthRequired);
    EXPECT_EQ(callback_count.load(), 0);

    ::close(fd);
    fixture.stop();
}

TEST(AuthIntegrationTest, CustomCallbackInvokedAfterAuth) {
    std::atomic<int> callback_count{0};

    AuthTestServer fixture;
    fixture.setAuthToken(kTestToken);
    fixture.setMessageCallback([&](mcp::TcpConnection* conn,
                                   const mcp::Message& msg) {
        if (!mcp::isRequest(msg)) {
            return;
        }
        callback_count.fetch_add(1);
        const auto& req = std::get<mcp::Request>(msg);
        auto resp = mcp::Response::success(req.id, {{"handled", "custom"}});
        conn->send(mcp::Message(std::move(resp)));
    });
    fixture.start();

    int fd = connectToServer();
    ASSERT_GE(fd, 0);

    auto auth = sendRequest(fd, "authenticate", {{"token", kTestToken}}, 1);
    ASSERT_TRUE(auth.has_value());
    EXPECT_FALSE(auth->contains("error"));

    auto resp = sendRequest(fd, "any/method", nlohmann::json::object(), 2);
    ASSERT_TRUE(resp.has_value());
    EXPECT_FALSE(resp->contains("error"));
    EXPECT_EQ((*resp)["result"]["handled"], "custom");
    EXPECT_EQ(callback_count.load(), 1);

    ::close(fd);
    fixture.stop();
}

} // anonymous namespace

#include "mcp/server/dispatcher.h"
#include "mcp/protocol/request.h"
#include "mcp/protocol/response.h"
#include "mcp/common/types.h"

#include <gtest/gtest.h>

namespace {

TEST(DispatcherTest, RegisterAndDispatch) {
    mcp::Dispatcher disp;

    disp.registerMethod("echo", [](const nlohmann::json& params) -> nlohmann::json {
        return {{"echo", params}};
    });

    EXPECT_TRUE(disp.hasMethod("echo"));
    EXPECT_FALSE(disp.hasMethod("nonexistent"));
    EXPECT_EQ(disp.methodCount(), 1u);

    mcp::Request req;
    req.method = "echo";
    req.params = {{"message", "hello"}};
    req.id = 1;

    auto resp = disp.dispatch(req);
    EXPECT_FALSE(resp.isError());
    EXPECT_EQ(resp.idString(), "1");
    EXPECT_EQ(resp.result["echo"]["message"], "hello");
}

TEST(DispatcherTest, MethodNotFound) {
    mcp::Dispatcher disp;

    mcp::Request req;
    req.method = "nonexistent";
    req.id = 1;

    auto resp = disp.dispatch(req);
    EXPECT_TRUE(resp.isError());
    EXPECT_EQ(resp.error->code, mcp::ErrorCodes::kMethodNotFound);
}

TEST(DispatcherTest, HandlerThrowsException) {
    mcp::Dispatcher disp;

    disp.registerMethod("failing", [](const nlohmann::json&) -> nlohmann::json {
        throw std::runtime_error("test error");
    });

    mcp::Request req;
    req.method = "failing";
    req.id = 1;

    auto resp = disp.dispatch(req);
    EXPECT_TRUE(resp.isError());
    EXPECT_EQ(resp.error->code, mcp::ErrorCodes::kInternalError);
}

TEST(DispatcherTest, OverwriteHandler) {
    mcp::Dispatcher disp;

    disp.registerMethod("test", [](const nlohmann::json&) {
        return nlohmann::json{{"version", 1}};
    });

    disp.registerMethod("test", [](const nlohmann::json&) {
        return nlohmann::json{{"version", 2}};
    });

    EXPECT_EQ(disp.methodCount(), 1u);

    mcp::Request req;
    req.method = "test";
    req.id = 1;

    auto resp = disp.dispatch(req);
    EXPECT_EQ(resp.result["version"], 2);
}

TEST(DispatcherTest, MultipleMethods) {
    mcp::Dispatcher disp;

    disp.registerMethod("add", [](const nlohmann::json& p) {
        return nlohmann::json(p["a"].get<int>() + p["b"].get<int>());
    });

    disp.registerMethod("sub", [](const nlohmann::json& p) {
        return nlohmann::json(p["a"].get<int>() - p["b"].get<int>());
    });

    EXPECT_EQ(disp.methodCount(), 2u);

    mcp::Request req1;
    req1.method = "add";
    req1.params = {{"a", 10}, {"b", 5}};
    req1.id = 1;

    auto resp1 = disp.dispatch(req1);
    EXPECT_EQ(resp1.result.get<int>(), 15);

    mcp::Request req2;
    req2.method = "sub";
    req2.params = {{"a", 10}, {"b", 5}};
    req2.id = 2;

    auto resp2 = disp.dispatch(req2);
    EXPECT_EQ(resp2.result.get<int>(), 5);
}

} // anonymous namespace

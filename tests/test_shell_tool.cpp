#include "plugins/shell/shell_tool.h"
#include "mcp/common/types.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace {

TEST(ShellToolTest, EchoSuccess) {
    mcp::ShellTool tool;
    mcp::Context ctx;
    nlohmann::json params = {{"command", "echo hello"}};

    auto result = tool.execute(params, ctx);
    EXPECT_FALSE(result.is_error);
    EXPECT_EQ(result.content["status"], "success");
    EXPECT_EQ(result.content["exit_code"], 0);
    EXPECT_NE(result.content["stdout"].get<std::string>().find("hello"),
              std::string::npos);
}

TEST(ShellToolTest, NonZeroExitCode) {
    mcp::ShellTool tool;
    mcp::Context ctx;
    nlohmann::json params = {{"command", "exit 42"}};

    auto result = tool.execute(params, ctx);
    EXPECT_FALSE(result.is_error);
    EXPECT_EQ(result.content["status"], "error");
    EXPECT_EQ(result.content["exit_code"], 42);
}

TEST(ShellToolTest, EmptyCommand) {
    mcp::ShellTool tool;
    mcp::Context ctx;
    nlohmann::json params = {{"command", ""}};

    auto result = tool.execute(params, ctx);
    EXPECT_TRUE(result.is_error);
    EXPECT_EQ(result.error->code, mcp::ErrorCodes::kInvalidParams);
}

TEST(ShellToolTest, MissingCommand) {
    mcp::ShellTool tool;
    mcp::Context ctx;
    nlohmann::json params = nlohmann::json::object();

    auto result = tool.execute(params, ctx);
    EXPECT_TRUE(result.is_error);
    EXPECT_EQ(result.error->code, mcp::ErrorCodes::kInvalidParams);
}

TEST(ShellToolTest, InvalidTimeout) {
    mcp::ShellTool tool;
    mcp::Context ctx;
    nlohmann::json params = {
        {"command", "echo ok"},
        {"timeout_seconds", 0}
    };

    auto result = tool.execute(params, ctx);
    EXPECT_TRUE(result.is_error);
    EXPECT_EQ(result.error->code, mcp::ErrorCodes::kInvalidParams);
}

TEST(ShellToolTest, CommandTimeout) {
    mcp::ShellTool tool(1);
    mcp::Context ctx;
    nlohmann::json params = {
        {"command", "sleep 5"},
        {"timeout_seconds", 1}
    };

    auto start = std::chrono::steady_clock::now();
    auto result = tool.execute(params, ctx);
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    EXPECT_FALSE(result.is_error);
    EXPECT_LT(elapsed_ms, 3000);
    EXPECT_EQ(result.content["exit_code"], -1);
    EXPECT_NE(result.content["stderr"].get<std::string>().find("[TIMEOUT]"),
              std::string::npos);
}

TEST(ShellToolTest, LargeOutputDoesNotDeadlock) {
    mcp::ShellTool tool(10, {"/bin/sh"}, 256 * 1024);
    mcp::Context ctx;
    nlohmann::json params = {
        {"command", "dd if=/dev/zero bs=1024 count=200 2>/dev/null | tr '\\0' 'A'"}
    };

    auto start = std::chrono::steady_clock::now();
    auto result = tool.execute(params, ctx);
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    EXPECT_FALSE(result.is_error);
    EXPECT_LT(elapsed_ms, 8000);
    EXPECT_GE(result.content["stdout"].get<std::string>().size(), 64 * 1024u);
}

TEST(ShellToolTest, InputSchema) {
    mcp::ShellTool tool(15);
    auto schema = tool.inputSchema();

    EXPECT_EQ(schema["type"], "object");
    EXPECT_TRUE(schema["properties"].contains("command"));
    EXPECT_TRUE(schema["properties"].contains("timeout_seconds"));
}

} // anonymous namespace

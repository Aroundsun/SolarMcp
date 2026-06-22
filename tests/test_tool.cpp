#include "mcp/tool/tool.h"
#include "mcp/tool/tool_manager.h"
#include "mcp/common/types.h"

#include <gtest/gtest.h>

namespace {

/// 简单测试工具
class EchoTool : public mcp::Tool {
public:
    std::string name() const override { return "echo"; }
    std::string description() const override { return "Echo back the input"; }

    nlohmann::json inputSchema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"message", {{"type", "string"}}}
            }}
        };
    }

    mcp::Result execute(const nlohmann::json& params, mcp::Context&) override {
        std::string msg = params.value("message", "");
        nlohmann::json result = {
            {"content", nlohmann::json::array({
                {{"type", "text"}, {"text", msg}}
            })}
        };
        return mcp::Result::ok(std::move(result));
    }
};

TEST(ToolManagerTest, RegisterAndGetTool) {
    mcp::ToolManager manager;

    auto tool = std::make_unique<EchoTool>();
    EXPECT_TRUE(manager.registerTool(std::move(tool)));
    EXPECT_EQ(manager.size(), 1u);

    auto* found = manager.getTool("echo");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name(), "echo");
    EXPECT_EQ(found->description(), "Echo back the input");
}

TEST(ToolManagerTest, DuplicateRegistrationFails) {
    mcp::ToolManager manager;

    EXPECT_TRUE(manager.registerTool(std::make_unique<EchoTool>()));
    EXPECT_FALSE(manager.registerTool(std::make_unique<EchoTool>()));
    EXPECT_EQ(manager.size(), 1u);
}

TEST(ToolManagerTest, GetToolNotFound) {
    mcp::ToolManager manager;
    EXPECT_EQ(manager.getTool("nonexistent"), nullptr);
}

TEST(ToolManagerTest, ListTools) {
    mcp::ToolManager manager;
    manager.registerTool(std::make_unique<EchoTool>());

    auto tools = manager.listTools();
    ASSERT_EQ(tools.size(), 1u);
    EXPECT_EQ(tools[0].name, "echo");
    EXPECT_EQ(tools[0].description, "Echo back the input");
    EXPECT_TRUE(tools[0].input_schema.is_object());
}

TEST(ToolManagerTest, CallTool) {
    mcp::ToolManager manager;
    manager.registerTool(std::make_unique<EchoTool>());

    mcp::Context ctx;
    nlohmann::json params = {{"message", "Hello Test"}};

    auto result = manager.callTool("echo", params, ctx);
    EXPECT_FALSE(result.is_error);
    EXPECT_TRUE(result.content.contains("content"));
    EXPECT_EQ(result.content["content"][0]["text"], "Hello Test");
}

TEST(ToolManagerTest, CallToolNotFound) {
    mcp::ToolManager manager;

    mcp::Context ctx;
    auto result = manager.callTool("nonexistent", {}, ctx);

    EXPECT_TRUE(result.is_error);
    EXPECT_EQ(result.error->code, mcp::ErrorCodes::kToolNotFound);
}

TEST(ToolManagerTest, EmptyManager) {
    mcp::ToolManager manager;
    EXPECT_TRUE(manager.empty());
    EXPECT_EQ(manager.size(), 0u);

    auto tools = manager.listTools();
    EXPECT_TRUE(tools.empty());
}

} // anonymous namespace

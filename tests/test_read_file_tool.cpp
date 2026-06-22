#include "mcp/tool/read_file_tool.h"
#include "mcp/common/types.h"

#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>

namespace {

class ReadFileToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "/tmp/solarmcp_test_readfile.txt";
        test_content_ = "Hello, SolarMcp!\nLine 2\nLine 3\n";

        std::ofstream out(test_file_);
        out << test_content_;
        out.close();
    }

    void TearDown() override {
        std::remove(test_file_.c_str());
    }

    std::string test_file_;
    std::string test_content_;
};

TEST_F(ReadFileToolTest, ReadExistingFile) {
    mcp::ReadFileTool tool(1024 * 1024, {"/tmp"});

    mcp::Context ctx;
    nlohmann::json params = {{"path", test_file_}};

    auto result = tool.execute(params, ctx);
    EXPECT_FALSE(result.is_error);
    EXPECT_TRUE(result.content.contains("content"));
    EXPECT_EQ(result.content["content"][0]["type"], "text");
    EXPECT_EQ(result.content["content"][0]["text"], test_content_);
}

TEST_F(ReadFileToolTest, FileNotFound) {
    mcp::ReadFileTool tool(1024 * 1024, {"/tmp"});

    mcp::Context ctx;
    nlohmann::json params = {{"path", "/tmp/nonexistent_file_abc.txt"}};

    auto result = tool.execute(params, ctx);
    EXPECT_TRUE(result.is_error);
    EXPECT_EQ(result.error->code, mcp::ErrorCodes::kToolError);
}

TEST_F(ReadFileToolTest, PathNotAllowed) {
    mcp::ReadFileTool tool(1024 * 1024, {"/home"}); // 仅允许 /home

    mcp::Context ctx;
    nlohmann::json params = {{"path", "/tmp/some_file.txt"}};

    auto result = tool.execute(params, ctx);
    EXPECT_TRUE(result.is_error);
    EXPECT_EQ(result.error->code, mcp::ErrorCodes::kToolError);
}

TEST_F(ReadFileToolTest, MissingPathParam) {
    mcp::ReadFileTool tool;

    mcp::Context ctx;
    nlohmann::json params = nlohmann::json::object(); // 无 "path"

    auto result = tool.execute(params, ctx);
    EXPECT_TRUE(result.is_error);
    EXPECT_EQ(result.error->code, mcp::ErrorCodes::kInvalidParams);
}

TEST_F(ReadFileToolTest, RelativePathRejected) {
    mcp::ReadFileTool tool;

    mcp::Context ctx;
    nlohmann::json params = {{"path", "relative/path"}};

    auto result = tool.execute(params, ctx);
    EXPECT_TRUE(result.is_error);
}

TEST_F(ReadFileToolTest, FileTooLarge) {
    // 最大 10 字节 — 测试文件更大
    mcp::ReadFileTool tool(10, {"/tmp"});

    mcp::Context ctx;
    nlohmann::json params = {{"path", test_file_}};

    auto result = tool.execute(params, ctx);
    EXPECT_TRUE(result.is_error);
    EXPECT_EQ(result.error->code, mcp::ErrorCodes::kToolError);
}

TEST_F(ReadFileToolTest, InputSchema) {
    mcp::ReadFileTool tool;
    auto schema = tool.inputSchema();

    EXPECT_EQ(schema["type"], "object");
    EXPECT_TRUE(schema.contains("properties"));
    EXPECT_TRUE(schema["properties"].contains("path"));
}

} // anonymous namespace

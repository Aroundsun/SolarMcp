#include "mcp/protocol/json_rpc_codec.h"
#include "mcp/protocol/request.h"
#include "mcp/protocol/response.h"
#include "mcp/protocol/message.h"
#include "mcp/network/buffer.h"

#include <gtest/gtest.h>

namespace {

// 辅助函数：创建带编码数据的缓冲区
std::string encodeFrame(const std::string& json_body) {
    return "Content-Length: " + std::to_string(json_body.size())
           + "\r\n\r\n" + json_body;
}

TEST(CodecTest, DecodeValidRequest) {
    mcp::JsonRpcCodec codec;
    mcp::Buffer buf;

    nlohmann::json req_json = {
        {"jsonrpc", "2.0"},
        {"method", "tools/list"},
        {"params", nlohmann::json::object()},
        {"id", 1}
    };

    std::string frame = encodeFrame(req_json.dump());
    buf.append(frame);

    auto msg = codec.decode(buf);
    ASSERT_TRUE(msg.has_value());
    ASSERT_TRUE(mcp::isRequest(msg.value()));

    const auto& req = std::get<mcp::Request>(msg.value());
    EXPECT_EQ(req.method, "tools/list");
    EXPECT_EQ(req.idString(), "1");
    EXPECT_EQ(buf.readableBytes(), 0u);
}

TEST(CodecTest, DecodeValidResponse) {
    mcp::JsonRpcCodec codec;
    mcp::Buffer buf;

    nlohmann::json resp_json = {
        {"jsonrpc", "2.0"},
        {"result", {{"tools", nlohmann::json::array()}}},
        {"id", 1}
    };

    std::string frame = encodeFrame(resp_json.dump());
    buf.append(frame);

    auto msg = codec.decode(buf);
    ASSERT_TRUE(msg.has_value());
    ASSERT_TRUE(mcp::isResponse(msg.value()));

    const auto& resp = std::get<mcp::Response>(msg.value());
    EXPECT_FALSE(resp.isError());
    EXPECT_EQ(resp.idString(), "1");
}

TEST(CodecTest, EncodeRequest) {
    mcp::JsonRpcCodec codec;

    mcp::Request req;
    req.method = "tools/call";
    req.params = {{"name", "read_file"}, {"arguments", {{"path", "/tmp/test"}}}};
    req.id = 42;

    std::string encoded = codec.encode(mcp::Message(req));

    // 验证 Content-Length 分帧
    EXPECT_NE(encoded.find("Content-Length: "), std::string::npos);
    EXPECT_NE(encoded.find("\r\n\r\n"), std::string::npos);
    EXPECT_NE(encoded.find("tools/call"), std::string::npos);
}

TEST(CodecTest, EncodeResponse) {
    mcp::JsonRpcCodec codec;

    auto resp = mcp::Response::success(1, {{"content", nlohmann::json::array()}});
    std::string encoded = codec.encode(mcp::Message(resp));

    EXPECT_NE(encoded.find("Content-Length: "), std::string::npos);
    EXPECT_NE(encoded.find("\"result\""), std::string::npos);
}

TEST(CodecTest, PartialDataReturnsNullopt) {
    mcp::JsonRpcCodec codec;
    mcp::Buffer buf;

    // 发送不完整的 Content-Length 头部
    buf.append("Content-Leng");
    auto msg = codec.decode(buf);
    EXPECT_FALSE(msg.has_value());

    // 数据应仍在缓冲区中（等待中）
    EXPECT_GT(buf.readableBytes(), 0u);
}

TEST(CodecTest, BodyNotYetComplete) {
    mcp::JsonRpcCodec codec;
    mcp::Buffer buf;

    std::string json_body = R"({"jsonrpc":"2.0","method":"test","id":1})";
    std::string header = "Content-Length: " + std::to_string(json_body.size()) + "\r\n\r\n";

    // 发送头部 + 部分 body
    buf.append(header);
    buf.append(json_body.substr(0, 10));

    auto msg = codec.decode(buf);
    EXPECT_FALSE(msg.has_value());

    // 发送剩余部分
    buf.append(json_body.substr(10));
    auto msg2 = codec.decode(buf);
    EXPECT_TRUE(msg2.has_value());
}

TEST(CodecTest, ParseErrorResponse) {
    mcp::JsonRpcCodec codec;
    mcp::Buffer buf;

    std::string invalid_json = "this is not json at all";
    std::string frame = encodeFrame(invalid_json);
    buf.append(frame);

    auto msg = codec.decode(buf);
    ASSERT_TRUE(msg.has_value());

    // 应产生错误 Response
    ASSERT_TRUE(mcp::isResponse(msg.value()));
    const auto& resp = std::get<mcp::Response>(msg.value());
    EXPECT_TRUE(resp.isError());
    EXPECT_EQ(resp.error->code, mcp::ErrorCodes::kParseError);
}

TEST(CodecTest, MultipleMessagesInOneBuffer) {
    mcp::JsonRpcCodec codec;
    mcp::Buffer buf;

    nlohmann::json msg1 = {{"jsonrpc", "2.0"}, {"method", "a"}, {"id", 1}};
    nlohmann::json msg2 = {{"jsonrpc", "2.0"}, {"method", "b"}, {"id", 2}};

    buf.append(encodeFrame(msg1.dump()));
    buf.append(encodeFrame(msg2.dump()));

    auto decoded1 = codec.decode(buf);
    ASSERT_TRUE(decoded1.has_value());
    ASSERT_TRUE(mcp::isRequest(decoded1.value()));
    EXPECT_EQ(std::get<mcp::Request>(decoded1.value()).method, "a");

    auto decoded2 = codec.decode(buf);
    ASSERT_TRUE(decoded2.has_value());
    ASSERT_TRUE(mcp::isRequest(decoded2.value()));
    EXPECT_EQ(std::get<mcp::Request>(decoded2.value()).method, "b");
}

} // anonymous namespace

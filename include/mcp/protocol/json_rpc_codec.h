#pragma once

#include "mcp/protocol/codec.h"
#include "mcp/network/buffer.h"

namespace mcp {

/// 带 Content-Length 分帧的 JSON-RPC 2.0 编解码器。
///
/// 线格式：
///   Content-Length: <N>\r\n
///   \r\n
///   <N 字节的 JSON-RPC 消息>
///
/// 这是 MCP（Model Context Protocol）使用的标准分帧。
class JsonRpcCodec : public Codec {
public:
    std::optional<Message> decode(Buffer& buffer) override;
    std::string encode(const Message& message) override;

private:
    /// 增量读取头部的解析状态。
    enum class ParseState {
        kWaitingForHeader,
        kWaitingForBody,
    };

    ParseState state_{ParseState::kWaitingForHeader};
    size_t expected_body_length_{0};
};

} // namespace mcp

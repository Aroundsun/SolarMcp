#pragma once

#include "mcp/protocol/message.h"

#include <optional>
#include <string>

namespace mcp {

// 前向声明
class Buffer;

/// 序列化/反序列化协议消息的抽象编解码器接口。
///
/// 实现类负责分帧（如 Content-Length 前缀）和
/// 序列化格式（如 JSON-RPC 2.0）。
class Codec {
public:
    virtual ~Codec() = default;

    /// 尝试从缓冲区解码一条 Message。
    /// 数据不足（部分消息）时返回 std::nullopt。
    /// 成功返回有效 Message；调用者应移除缓冲区中已消费的字节。
    virtual std::optional<Message> decode(Buffer& buffer) = 0;

    /// 将 Message 编码为线格式字符串（含分帧）。
    virtual std::string encode(const Message& message) = 0;
};

} // namespace mcp

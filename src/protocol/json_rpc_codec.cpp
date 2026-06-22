#include "mcp/protocol/json_rpc_codec.h"
#include "mcp/protocol/request.h"
#include "mcp/protocol/response.h"

#include <nlohmann/json.hpp>
#include <sstream>

namespace mcp {

// ---------------------------------------------------------------------------
// decode — Content-Length 分帧 + JSON-RPC 2.0 解析
// ---------------------------------------------------------------------------

std::optional<Message> JsonRpcCodec::decode(Buffer& buffer) {
    while (true) {
        if (state_ == ParseState::kWaitingForHeader) {
            // 查找 "Content-Length: <N>\r\n"
            ssize_t crlf_pos = buffer.findCRLF();
            if (crlf_pos < 0) {
                return std::nullopt; // 需要更多数据
            }

            std::string header_line = buffer.retrieveAsString(
                static_cast<size_t>(crlf_pos));
            buffer.retrieve(2); // 消费 "\r\n"

            // 解析 "Content-Length: <N>"
            const std::string prefix = "Content-Length: ";
            if (header_line.size() > prefix.size() &&
                header_line.compare(0, prefix.size(), prefix) == 0) {
                std::string num_str = header_line.substr(prefix.size());
                try {
                    expected_body_length_ = std::stoul(num_str);
                } catch (const std::invalid_argument&) {
                    // Content-Length 格式错误 — 返回解析错误
                    Response err = Response::makeError(
                        nullptr, ErrorCodes::kParseError,
                        "Invalid Content-Length value: " + num_str);
                    return Message(std::move(err));
                } catch (const std::out_of_range&) {
                    // Content-Length 值过大
                    Response err = Response::makeError(
                        nullptr, ErrorCodes::kParseError,
                        "Content-Length value out of range: " + num_str);
                    return Message(std::move(err));
                }
                state_ = ParseState::kWaitingForBody;
                // 继续读取 body
            } else {
                // 无效头部 — 跳过此行并重试
                continue;
            }
        }

        if (state_ == ParseState::kWaitingForBody) {
            // 头部之后期望 "\r\n" 分隔符
            if (buffer.readableBytes() >= 2) {
                // 检查 "\r\n"
                const char* peek_ptr = buffer.peek();
                if (peek_ptr[0] == '\r' && peek_ptr[1] == '\n') {
                    buffer.retrieve(2);
                }
                // 等待完整 body
            }

            // 等待完整 body 就绪
            if (buffer.readableBytes() < expected_body_length_) {
                return std::nullopt;
            }

            std::string body = buffer.retrieveAsString(expected_body_length_);
            state_ = ParseState::kWaitingForHeader;
            expected_body_length_ = 0;

            // 解析 JSON
            try {
                nlohmann::json j = nlohmann::json::parse(body);

                // 判断消息类型
                if (j.contains("method")) {
                    // 为 Request（或 Notification）
                    Request req = Request::fromJson(j);
                    return Message(std::move(req));
                } else if (j.contains("result") || j.contains("error")) {
                    // 为 Response
                    Response resp = Response::fromJson(j);
                    return Message(std::move(resp));
                } else {
                    // 未知消息类型 — 返回错误响应
                    Response err = Response::makeError(
                        nullptr, ErrorCodes::kInvalidRequest,
                        "Message is neither a valid Request nor Response");
                    return Message(std::move(err));
                }
            } catch (const nlohmann::json::parse_error& e) {
                // JSON 解析错误
                Response err = Response::makeError(
                    nullptr, ErrorCodes::kParseError,
                    std::string("Parse error: ") + e.what());
                return Message(std::move(err));
            } catch (const std::exception& e) {
                Response err = Response::makeError(
                    nullptr, ErrorCodes::kInvalidRequest,
                    std::string("Invalid request: ") + e.what());
                return Message(std::move(err));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// encode — JSON-RPC 2.0 + Content-Length 分帧
// ---------------------------------------------------------------------------

std::string JsonRpcCodec::encode(const Message& message) {
    nlohmann::json j;

    std::visit([&j](const auto& msg) {
        j = msg.toJson();
    }, message);

    std::string body = j.dump();

    std::ostringstream oss;
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "\r\n";
    oss << body;

    return oss.str();
}

} // namespace mcp

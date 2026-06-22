#include "mcp/protocol/response.h"

namespace mcp {

Response Response::success(
    std::variant<std::string, int, std::nullptr_t> id,
    nlohmann::json result) {
    Response resp;
    resp.id = std::move(id);
    resp.result = std::move(result);
    return resp;
}

Response Response::makeError(
    std::variant<std::string, int, std::nullptr_t> id,
    int code, std::string message) {
    Response resp;
    resp.id = std::move(id);
    resp.error = ErrorInfo{code, std::move(message)};
    return resp;
}

Response Response::fromJson(const nlohmann::json& j) {
    Response resp;

    if (j.contains("jsonrpc")) {
        resp.jsonrpc = j["jsonrpc"].get<std::string>();
    }

    if (j.contains("result")) {
        resp.result = j["result"];
    }

    if (j.contains("error")) {
        const auto& err = j["error"];
        ErrorInfo info;
        info.code = err.value("code", 0);
        info.message = err.value("message", "");
        resp.error = std::move(info);
    }

    if (j.contains("id")) {
        const auto& id_field = j["id"];
        if (id_field.is_string()) {
            resp.id = id_field.get<std::string>();
        } else if (id_field.is_number_integer()) {
            resp.id = id_field.get<int>();
        } else if (id_field.is_null()) {
            resp.id = nullptr;
        }
    }

    return resp;
}

nlohmann::json Response::toJson() const {
    nlohmann::json j;
    j["jsonrpc"] = jsonrpc;

    if (error.has_value()) {
        j["error"] = {
            {"code", error->code},
            {"message", error->message}
        };
    } else {
        j["result"] = result;
    }
    // JSON-RPC 2.0 规范：id 为 null 时必须显式输出 "id": null
    std::visit([&j](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            j["id"] = nullptr;
        } else {
            j["id"] = v;
        }
    }, id);


    return j;
}

std::string Response::idString() const {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<T, int>) {
            return std::to_string(v);
        } else {
            return "";
        }
    }, id);
}

} // namespace mcp

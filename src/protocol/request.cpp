#include "mcp/protocol/request.h"

#include <stdexcept>

namespace mcp {

Request Request::fromJson(const nlohmann::json& j) {
    Request req;

    if (j.contains("jsonrpc")) {
        req.jsonrpc = j["jsonrpc"].get<std::string>();
    }

    if (!j.contains("method")) {
        throw std::runtime_error("Request missing 'method' field");
    }
    req.method = j["method"].get<std::string>();

    if (j.contains("params")) {
        req.params = j["params"];
    }

    if (j.contains("id")) {
        const auto& id_field = j["id"];
        if (id_field.is_string()) {
            req.id = id_field.get<std::string>();
        } else if (id_field.is_number_integer()) {
            req.id = id_field.get<int>();
        } else if (id_field.is_null()) {
            req.id = nullptr;
        }
    } else {
        // Notification：无 id
        req.id = nullptr;
    }

    return req;
}

nlohmann::json Request::toJson() const {
    nlohmann::json j;
    j["jsonrpc"] = jsonrpc;
    j["method"] = method;
    j["params"] = params;

    std::visit([&j](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            // Notification — 省略 id
        } else {
            j["id"] = v;
        }
    }, id);

    return j;
}

bool Request::isNotification() const {
    return std::holds_alternative<std::nullptr_t>(id);
}

std::string Request::idString() const {
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

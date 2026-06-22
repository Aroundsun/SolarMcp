#include "mcp/plugin/callback_tool.h"

#include "mcp/common/types.h"

#include <cstdlib>
#include <cstring>

namespace mcp {

CallbackTool::CallbackTool(std::string name,
                           std::string description,
                           std::string input_schema_json,
                           mcp_tool_execute_fn execute_fn)
    : name_(std::move(name))
    , description_(std::move(description))
    , input_schema_json_(std::move(input_schema_json))
    , execute_fn_(execute_fn)
{}

nlohmann::json CallbackTool::inputSchema() const {
    if (input_schema_json_.empty()) {
        return {{"type", "object"}, {"properties", nlohmann::json::object()}};
    }
    return nlohmann::json::parse(input_schema_json_);
}

Result CallbackTool::execute(const nlohmann::json& params, Context& /*ctx*/) {
    if (!execute_fn_) {
        return Result::err(ErrorCodes::kToolError, "Plugin execute callback is null");
    }

    std::string params_json = params.dump();
    char* out_json = nullptr;
    char* err_msg = nullptr;

    int rc = execute_fn_(params_json.c_str(), &out_json, &err_msg);
    if (rc != MCP_PLUGIN_OK) {
        std::string msg = err_msg ? err_msg : "Plugin execute failed";
        if (err_msg) {
            ::free(err_msg);
        }
        return Result::err(ErrorCodes::kToolError, std::move(msg));
    }

    if (!out_json) {
        return Result::ok(nlohmann::json::object());
    }

    nlohmann::json content;
    try {
        content = nlohmann::json::parse(out_json);
    } catch (const std::exception& e) {
        ::free(out_json);
        return Result::err(ErrorCodes::kToolError,
                           std::string("Invalid plugin output JSON: ") + e.what());
    }
    ::free(out_json);
    return Result::ok(std::move(content));
}

} // namespace mcp

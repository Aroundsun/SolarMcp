#include "mcp/server/dispatcher.h"
#include "mcp/common/types.h"

namespace mcp {

// ---------------------------------------------------------------------------
// registerMethod
// ---------------------------------------------------------------------------

void Dispatcher::registerMethod(const std::string& method, Handler handler) {
    handlers_[method] = std::move(handler);
}

// ---------------------------------------------------------------------------
// hasMethod
// ---------------------------------------------------------------------------

bool Dispatcher::hasMethod(const std::string& method) const {
    return handlers_.find(method) != handlers_.end();
}

// ---------------------------------------------------------------------------
// dispatch
// ---------------------------------------------------------------------------

Response Dispatcher::dispatch(const Request& request) {
    // 查找处理函数
    auto it = handlers_.find(request.method);
    if (it == handlers_.end()) {
        return Response::makeError(
            request.id,
            ErrorCodes::kMethodNotFound,
            "Method not found: " + request.method);
    }

    try {
        nlohmann::json result = it->second(request.params);
        return Response::success(request.id, std::move(result));
    } catch (const std::exception& e) {
        return Response::makeError(
            request.id,
            ErrorCodes::kInternalError,
            std::string("Internal error: ") + e.what());
    }
}

} // namespace mcp

#pragma once

namespace mcp {

/// 禁用拷贝语义但允许移动语义的基类。
/// 适用于不应被复制的 RAII 资源持有类。
class NonCopyable {
public:
    NonCopyable() = default;
    ~NonCopyable() = default;

    // 禁用拷贝
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

    // 允许移动
    NonCopyable(NonCopyable&&) noexcept = default;
    NonCopyable& operator=(NonCopyable&&) noexcept = default;
};

} // namespace mcp

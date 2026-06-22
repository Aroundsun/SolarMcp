#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace mcp {

/// 带可前置 / 可读 / 可写区域的网络缓冲区。
///
/// 布局：
///   [ prependable (8B) | readable (readIdx_ → writeIdx_) | writable ]
///
/// 每个连接拥有独立的输入和输出缓冲区。
/// 线程安全：非线程安全 — 每个 Buffer 属于单一所有者。
class Buffer {
public:
    /// 为协议头预留的初始可前置字节数。
    static constexpr size_t kPrependSize = 8;

    /// 默认初始容量。
    static constexpr size_t kInitialSize = 1024;

    Buffer();

    // ---- 读操作 ----

    /// 将原始数据追加到可写区域。
    void append(const char* data, size_t len);

    /// 从字符串追加数据。
    void append(const std::string& str);

    /// 从可读区域前端消费 `len` 字节。
    void retrieve(size_t len);

    /// 消费全部可读数据。
    void retrieveAll();

    /// 消费直到（含）给定指针的所有字节。
    void retrieveUntil(const char* end);

    /// 窥视可读数据但不消费。
    const char* peek() const;

    /// 将完整可读区域作为字符串返回。
    std::string retrieveAsString(size_t len);

    /// 返回全部可读数据为字符串并消费。
    std::string retrieveAllAsString();

    // ---- 长度查询 ----

    /// 可读字节数。
    size_t readableBytes() const noexcept { return write_idx_ - read_idx_; }

    /// 可写字节数（调整大小前）。
    size_t writableBytes() const noexcept { return buffer_.size() - write_idx_; }

    /// 可前置字节数（可读区域之前）。
    size_t prependableBytes() const noexcept { return read_idx_; }

    // ---- 写辅助 ----

    /// 在可读区域之前前置数据。
    void prepend(const char* data, size_t len);

    /// 确保至少 `len` 字节可写，必要时调整大小。
    void ensureWritableBytes(size_t len);

    /// 在可读数据中查找 CRLF（"\r\n"）位置。
    /// 返回相对 peek() 的偏移，未找到返回 -1。
    ssize_t findCRLF() const;

    // ---- 访问器 ----

    /// 缓冲区起始原始指针。
    char* begin() noexcept { return buffer_.data(); }
    const char* begin() const noexcept { return buffer_.data(); }

    /// 可读数据起始指针。
    char* peekPtr() noexcept { return begin() + read_idx_; }

    /// 可写空间起始指针。
    char* writePtr() noexcept { return begin() + write_idx_; }
    const char* writePtr() const noexcept { return begin() + write_idx_; }

    /// 当前总容量。
    size_t capacity() const noexcept { return buffer_.size(); }

    /// 准备可写空间并返回可变指针和长度。
    /// 调用者写入后调用 hasWritten() 推进 write_idx_。
    char* prepareWrite(size_t len);
    void hasWritten(size_t len);

    /// 重置为空状态（不释放内存）。
    void clear();

private:
    /// 将可读数据移到前端以回收空间。
    void makeSpace(size_t len);

    std::vector<char> buffer_;
    size_t read_idx_{kPrependSize};
    size_t write_idx_{kPrependSize};
};

} // namespace mcp

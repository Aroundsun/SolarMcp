#include "mcp/network/buffer.h"

#include <cassert>
#include <cstring>

namespace mcp {

// ---------------------------------------------------------------------------
// 构造
// ---------------------------------------------------------------------------

Buffer::Buffer() {
    buffer_.resize(kInitialSize);
}

// ---------------------------------------------------------------------------
// append
// ---------------------------------------------------------------------------

void Buffer::append(const char* data, size_t len) {
    if (len == 0) return;
    ensureWritableBytes(len);
    std::memcpy(writePtr(), data, len);
    write_idx_ += len;
}

void Buffer::append(const std::string& str) {
    append(str.data(), str.size());
}

// ---------------------------------------------------------------------------
// retrieve
// ---------------------------------------------------------------------------

void Buffer::retrieve(size_t len) {
    assert(len <= readableBytes());
    if (len < readableBytes()) {
        read_idx_ += len;
    } else {
        retrieveAll();
    }
}

void Buffer::retrieveAll() {
    read_idx_ = kPrependSize;
    write_idx_ = kPrependSize;
}

void Buffer::retrieveUntil(const char* end) {
    assert(peek() <= end);
    assert(end <= writePtr());
    retrieve(static_cast<size_t>(end - peek()));
}

// ---------------------------------------------------------------------------
// peek / retrieveAsString
// ---------------------------------------------------------------------------

const char* Buffer::peek() const {
    return begin() + read_idx_;
}

std::string Buffer::retrieveAsString(size_t len) {
    assert(len <= readableBytes());
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

std::string Buffer::retrieveAllAsString() {
    return retrieveAsString(readableBytes());
}

// ---------------------------------------------------------------------------
// prepend
// ---------------------------------------------------------------------------

void Buffer::prepend(const char* data, size_t len) {
    assert(len <= prependableBytes());
    read_idx_ -= len;
    std::memcpy(begin() + read_idx_, data, len);
}

// ---------------------------------------------------------------------------
// ensureWritableBytes / makeSpace
// ---------------------------------------------------------------------------

void Buffer::ensureWritableBytes(size_t len) {
    if (writableBytes() >= len) {
        return;
    }
    // 先通过紧凑回收空间
    if (writableBytes() + prependableBytes() >= len + kPrependSize) {
        makeSpace(len);
    } else {
        // 需要扩容
        buffer_.resize(write_idx_ + len);
    }
}

void Buffer::makeSpace(size_t /*len*/) {
    size_t readable = readableBytes();
    std::memmove(begin() + kPrependSize, peek(), readable);
    read_idx_ = kPrependSize;
    write_idx_ = read_idx_ + readable;
}

// ---------------------------------------------------------------------------
// findCRLF
// ---------------------------------------------------------------------------

ssize_t Buffer::findCRLF() const {
    const char* start = peek();
    const size_t len = readableBytes();
    if (len < 2) {
        return -1;
    }
    for (size_t i = 0; i + 1 < len; ++i) {
        if (start[i] == '\r' && start[i + 1] == '\n') {
            return static_cast<ssize_t>(i);
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// prepareWrite / hasWritten
// ---------------------------------------------------------------------------

char* Buffer::prepareWrite(size_t len) {
    ensureWritableBytes(len);
    return writePtr();
}

void Buffer::hasWritten(size_t len) {
    assert(len <= writableBytes());
    write_idx_ += len;
}

// ---------------------------------------------------------------------------
// clear
// ---------------------------------------------------------------------------

void Buffer::clear() {
    read_idx_ = kPrependSize;
    write_idx_ = kPrependSize;
}

} // namespace mcp

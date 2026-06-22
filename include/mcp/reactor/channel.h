#pragma once

#include "mcp/common/noncopyable.h"

#include <functional>
#include <memory>

namespace mcp {

class EventLoop;

/// 封装文件描述符、关注的 I/O 事件及回调。
///
/// Channel 不拥有 fd — 所有者（如 TcpConnection）负责关闭。
/// Channel 向 EventLoop 注册并通过 handleEvent() 接收事件通知。
///
/// 线程安全：仅能在所属 EventLoop 线程调用。
class Channel : public NonCopyable {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    // ---- 回调设置 ----

    void setReadCallback(EventCallback cb)  { read_callback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { write_callback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { close_callback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { error_callback_ = std::move(cb); }

    // ---- 事件管理 ----

    /// EventLoop 在此 fd 发生事件时调用。
    void handleEvent();

    /// 启用读监控（EPOLLIN）。
    void enableReading();

    /// 启用写监控（EPOLLOUT）。
    void enableWriting();

    /// 禁用读监控。
    void disableReading();

    /// 禁用写监控。
    void disableWriting();

    /// 禁用全部事件监控。
    void disableAll();

    /// 标记此 channel 从 EventLoop 移除。
    void remove();

    // ---- 访问器 ----

    int fd() const noexcept { return fd_; }
    uint32_t events() const noexcept { return events_; }
    EventLoop* ownerLoop() const noexcept { return loop_; }

    /// 设置 epoll 的「已接收事件」（由 EpollPoller 调用）。
    void setRevents(uint32_t revents) noexcept { revents_ = revents; }

    /// 返回当前事件状态索引（EpollPoller 使用）。
    int index() const noexcept { return index_; }
    void setIndex(int idx) noexcept { index_ = idx; }

    /// 未监控任何事件时返回 true。
    bool isNoneEvent() const noexcept { return events_ == kNoneEvent; }

    // 事件标志
    static constexpr uint32_t kNoneEvent  = 0;
    static constexpr uint32_t kReadEvent  = 1;  // EPOLLIN
    static constexpr uint32_t kWriteEvent = 2;  // EPOLLOUT (not EPOLLIN|EPOLLOUT for edge trigger)

private:
    void update();

    EventLoop* loop_;
    int fd_;
    uint32_t events_{kNoneEvent};
    uint32_t revents_{kNoneEvent};

    int index_{-1};  // EpollPoller state index

    EventCallback read_callback_;
    EventCallback write_callback_;
    EventCallback close_callback_;
    EventCallback error_callback_;
};

} // namespace mcp

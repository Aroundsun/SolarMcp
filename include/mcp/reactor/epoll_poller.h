#pragma once

#include "mcp/reactor/poller.h"

#include <sys/epoll.h>
#include <vector>

namespace mcp {

/// 基于 Linux epoll 的 Poller 实现。
///
/// 使用 epoll_create1(EPOLL_CLOEXEC) 创建 fd，
/// 并以 EPOLLET（边缘触发）模式实现高性能 I/O。
class EpollPoller : public Poller {
public:
    EpollPoller();
    ~EpollPoller() override;

    std::int64_t poll(int timeout_ms,
                 std::vector<Channel*>& active_channels) override;

    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    /// 每次 epoll_wait 返回的最大事件数。
    static constexpr int kMaxEvents = 128;

    /// 从 epoll 事件填充 active_channels。
    void fillActiveChannels(int num_events,
                            std::vector<Channel*>& active_channels);

    /// 底层 epoll_ctl 包装。
    void epollCtl(int operation, Channel* channel);

    int epoll_fd_{-1};
    std::vector<epoll_event> events_;
};

} // namespace mcp

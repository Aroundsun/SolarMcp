#include "mcp/reactor/epoll_poller.h"
#include "mcp/reactor/channel.h"

#include <chrono>
#include <cstring>
#include <unistd.h>

namespace mcp {

// ---------------------------------------------------------------------------
// 构造 / 析构
// ---------------------------------------------------------------------------

EpollPoller::EpollPoller() {
    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        throw std::runtime_error("EpollPoller: epoll_create1 failed");
    }
    events_.resize(kMaxEvents);
}

EpollPoller::~EpollPoller() {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

// ---------------------------------------------------------------------------
// poll
// ---------------------------------------------------------------------------

std::int64_t EpollPoller::poll(int timeout_ms,
                          std::vector<Channel*>& active_channels) {
    int num_events = ::epoll_wait(epoll_fd_, events_.data(),
                                   static_cast<int>(events_.size()),
                                   timeout_ms);

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (num_events > 0) {
        fillActiveChannels(num_events, active_channels);

        // 缓冲区满时扩容事件数组
        if (static_cast<size_t>(num_events) == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    }

    return now;
}

// ---------------------------------------------------------------------------
// fillActiveChannels
// ---------------------------------------------------------------------------

void EpollPoller::fillActiveChannels(int num_events,
                                     std::vector<Channel*>& active_channels) {
    for (int i = 0; i < num_events; ++i) {
        auto* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->setRevents(events_[i].events);
        active_channels.push_back(channel);
    }
}

// ---------------------------------------------------------------------------
// updateChannel
// ---------------------------------------------------------------------------

void EpollPoller::updateChannel(Channel* channel) {
    int index = channel->index();
    if (index < 0) {
        // 新 channel — 添加
        channel->setIndex(1); // Mark as "added"
        epollCtl(EPOLL_CTL_ADD, channel);
    } else {
        // 已有 channel — 修改
        if (channel->isNoneEvent()) {
            epollCtl(EPOLL_CTL_DEL, channel);
            channel->setIndex(-1);
        } else {
            epollCtl(EPOLL_CTL_MOD, channel);
        }
    }
}

// ---------------------------------------------------------------------------
// removeChannel
// ---------------------------------------------------------------------------

void EpollPoller::removeChannel(Channel* channel) {
    int index = channel->index();
    if (index >= 0) {
        epollCtl(EPOLL_CTL_DEL, channel);
        channel->setIndex(-1);
    }
}

// ---------------------------------------------------------------------------
// epollCtl — 底层系统调用包装
// ---------------------------------------------------------------------------

void EpollPoller::epollCtl(int operation, Channel* channel) {
    epoll_event event{};
    std::memset(&event, 0, sizeof(event));
    event.events = channel->events();
    event.data.ptr = channel;

    if (::epoll_ctl(epoll_fd_, operation, channel->fd(), &event) < 0) {
        // channel 生命周期切换时可能出现 EEXIST 或 ENOENT
        // 在边缘触发模式下通常无害
    }
}

} // namespace mcp

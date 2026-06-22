#pragma once

#include <cstdint>
#include <vector>

namespace mcp {

class Channel;

/// 抽象 I/O 多路复用接口。
///
/// 具体实现封装操作系统特定的轮询机制
///（Linux 上为 epoll，macOS/BSD 上为 kqueue 等）。
class Poller {
public:
    virtual ~Poller() = default;

    /// 带超时等待事件。
    /// @param timeout_ms   最大等待时间（毫秒）
    /// @param active_channels  输出：有待处理事件的 channel
    /// @return poll 返回时间戳（用于计时）
    virtual std::int64_t poll(int timeout_ms,
                         std::vector<Channel*>& active_channels) = 0;

    /// 注册或更新 channel 的事件关注。
    virtual void updateChannel(Channel* channel) = 0;

    /// 从 poller 移除 channel。
    virtual void removeChannel(Channel* channel) = 0;
};

} // namespace mcp

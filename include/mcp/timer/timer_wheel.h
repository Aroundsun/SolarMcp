#pragma once

#include <chrono>
#include <functional>
#include <list>
#include <vector>

namespace mcp {

/// 分层时间轮，O(1) 插入与取消定时器。
///
/// 每个槽位持有 TimerEntry 列表。轮 tick 时，
/// 当前槽位全部条目触发并前进一格。
///
/// 用途：工具执行超时、心跳检测、空闲连接清理。
class TimerWheel {
public:
    using TimerCallback = std::function<void()>;

    /// 单个定时器条目。
    struct TimerEntry {
        int64_t id;                  // Unique timer ID
        int64_t expiration;         // Absolute tick at which this fires
        TimerCallback callback;     // Function to invoke

        bool operator==(const TimerEntry& other) const {
            return id == other.id;
        }
    };

    /// 构造时间轮。
    /// @param tick_ms   每 tick 时长（毫秒）
    /// @param num_slots  槽位总数（轮大小）
    TimerWheel(int tick_ms = 100, int num_slots = 600);

    /// 添加定时器，返回唯一 ID。
    /// @param delay_ms   从现在到到期的延迟
    /// @param callback   到期时调用的函数
    /// @return 唯一定时器 ID，错误时 -1
    int64_t addTimer(int delay_ms, TimerCallback callback);

    /// 按 ID 取消定时器。
    /// @return 找到并取消时返回 true
    bool cancelTimer(int64_t timer_id);

    /// 前进一格。触发当前槽位全部定时器。
    /// 返回触发的定时器数量。
    int tick();

    /// 获取当前槽位索引。
    int currentSlot() const noexcept { return current_slot_; }

    /// 活跃定时器数量。
    size_t activeCount() const noexcept { return active_count_; }

private:
    int tick_ms_;
    int num_slots_;
    int current_slot_{0};
    int64_t next_timer_id_{1};
    size_t active_count_{0};

    /// 每个槽位为定时器条目列表。
    std::vector<std::list<TimerEntry>> slots_;

    /// 单调递增 tick 计数（不重置）。
    int64_t tick_count_{0};

    /// 计算给定到期 tick 的槽位索引。
    int slotIndex(int64_t expiration_tick) const noexcept {
        return static_cast<int>(expiration_tick % num_slots_);
    }
};

} // namespace mcp

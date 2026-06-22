#include "mcp/timer/timer_wheel.h"

namespace mcp {

// ---------------------------------------------------------------------------
// 构造
// ---------------------------------------------------------------------------

TimerWheel::TimerWheel(int tick_ms, int num_slots)
    : tick_ms_(tick_ms)
    , num_slots_(num_slots)
{
    slots_.resize(static_cast<size_t>(num_slots_));
}

// ---------------------------------------------------------------------------
// addTimer
// ---------------------------------------------------------------------------

int64_t TimerWheel::addTimer(int delay_ms, TimerCallback callback) {
    if (delay_ms <= 0 || !callback) {
        return -1;
    }

    // 将 delay_ms 转换为 tick 数（向上取整）
    int64_t delay_ticks = (delay_ms + tick_ms_ - 1) / tick_ms_;
    int64_t expiration = tick_count_ + delay_ticks;
    int64_t id = next_timer_id_++;

    TimerEntry entry;
    entry.id = id;
    entry.expiration = expiration;
    entry.callback = std::move(callback);

    int slot = slotIndex(expiration);
    slots_[static_cast<size_t>(slot)].push_back(std::move(entry));
    ++active_count_;

    return id;
}

// ---------------------------------------------------------------------------
// cancelTimer
// ---------------------------------------------------------------------------

bool TimerWheel::cancelTimer(int64_t timer_id) {
    // 已取消的定时器可能在任意槽位，需要搜索。
    // 为简单起见遍历所有槽位；生产环境可维护
    // timer_id → slot+iterator 的哈希表以实现 O(1) 取消。
    for (auto& slot_list : slots_) {
        for (auto it = slot_list.begin(); it != slot_list.end(); ++it) {
            if (it->id == timer_id) {
                slot_list.erase(it);
                --active_count_;
                return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// tick
// ---------------------------------------------------------------------------

int TimerWheel::tick() {
    int fired = 0;

    auto& slot_list = slots_[static_cast<size_t>(current_slot_)];

    // 遍历并触发已到期的定时器
    auto it = slot_list.begin();
    while (it != slot_list.end()) {
        if (it->expiration <= tick_count_) {
            // 触发回调
            if (it->callback) {
                it->callback();
            }
            it = slot_list.erase(it);
            --active_count_;
            ++fired;
        } else {
            ++it;
        }
    }

    // 时间轮前进一格
    ++tick_count_;
    current_slot_ = static_cast<int>(tick_count_ % num_slots_);

    return fired;
}

} // namespace mcp

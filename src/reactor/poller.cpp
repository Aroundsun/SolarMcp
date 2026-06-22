// Poller 是纯抽象接口，除链接器所需外无需 .cpp 实现。

#include "mcp/reactor/poller.h"

// 此空翻译单元确保 vtable 被生成。
namespace mcp {
    // Poller 的 vtable 占位 — 实际实现在 EpollPoller 中
}

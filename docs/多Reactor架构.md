# 多 Reactor 架构

## 架构

```
Client ──→ Main Reactor (accept)
              │
              ├── Round-Robin ──→ Worker Reactor #0..N (连接 IO)
              │                         │
              │                         ├── authenticate / tools/list（IO 线程同步）
              │                         └── tools/call / plugins/reload
              │                                    │
              └────────────────────────────────────┘
                                                   ▼
                                          ThreadPool（业务线程）
                                                   │
                                          sendInLoop → Worker IO 线程写回
```

## 配置

```yaml
reactor:
  mode: "multi"      # "single" 回退单 Reactor
  io_threads: 4      # Worker Reactor 数量

thread_pool:
  worker_threads: 4
  max_queue_size: 10000
```

- `mode: single` 或 `io_threads: 0`：与 P0 相同，Main Reactor 处理全部 IO。
- `tools/call`、`plugins/reload` 投递到 ThreadPool；队列满返回 `-32005 Server busy`。
- `ToolManager` / `PluginManager` 使用读写锁，支持并发 call 与 reload。

## 关键类

| 类 | 职责 |
|----|------|
| `EventLoopThread` | 独立线程运行一个 `EventLoop` |
| `EventLoopThreadPool` | N 个 Worker Loop，Round-Robin 分配新连接 |
| `TcpServer` | Main accept + 连接分配到 Worker |
| `TcpConnection::sendInLoop` | 跨线程安全写回响应 |

## 关闭顺序

```
base_loop.quit() → server.stop() → worker_pool.stop()
→ plugin unload → thread_pool.shutdown()
```

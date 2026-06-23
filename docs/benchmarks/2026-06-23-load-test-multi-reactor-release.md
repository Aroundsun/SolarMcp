# SolarMcp 压测报告（Multi-Reactor Release）

## 环境

| 项 | 值 |
|----|-----|
| 时间 | 2026-06-23 多 Reactor 升级后 |
| 目标 | 127.0.0.1:8090 |
| 构建 | Release（`build-release/`） |
| 架构 | **Main Reactor + 4 Worker Reactor + 4 ThreadPool** |
| 配置 | `reactor.mode: multi`, `reactor.io_threads: 4` |
| 机器 | VMware VM, x86_64, 4 CPU, 3.3Gi RAM |
| 对比基线 | [2026-06-23-load-test-release.md](2026-06-23-load-test-release.md)（单 Reactor Release） |

原始 JSON：[`2026-06-23-load-test-multi-reactor-release.json`](2026-06-23-load-test-multi-reactor-release.json)

## 升级前 vs 升级后（Release 对 Release）

| 场景 | 指标 | 单 Reactor | Multi-Reactor | 变化 |
|------|------|-----------|---------------|------|
| A. tools/list 基线 (10×200) | QPS | 4671 | 4517 | **-3.3%** |
| | p99 | 7.9ms | 7.7ms | -2.5% |
| B. tools/list 高并发 (50×100) | QPS | 4520 | 4078 | **-9.8%** |
| | p99 | 38.6ms | 42.5ms | +10.1% |
| C. read_file (20×50) | QPS | 4269 | 3890 | **-8.9%** |
| | p99 | 12.5ms | 14.8ms | +18.7% |
| D. plugin_demo (20×50) | QPS | 4816 | 4131 | **-14.2%** |
| | p99 | 13.5ms | 15.3ms | +13.0% |

> 所有场景 **0 失败**。

## 结果摘要

| 场景 | 连接×请求 | QPS | p50 (ms) | p99 (ms) |
|------|-----------|-----|----------|----------|
| A. tools/list 基线 | 10×200 | 4517.0 | 1.6 | 7.7 |
| B. tools/list 高并发 | 50×100 | 4077.7 | 5.8 | 42.5 |
| C. read_file | 20×50 | 3889.5 | 3.2 | 14.8 |
| D. plugin_demo | 20×50 | 4130.6 | 2.9 | 15.3 |

## 分析与结论

### 1. 轻量 RPC 吞吐略降（符合预期）

升级后 `tools/call` 走 **ThreadPool 异步路径**（enqueue → 业务线程 execute → sendInLoop 回 IO 线程），比单 Reactor 同步调用多两次线程切换，因此 C/D 场景 QPS 下降约 **9–14%**，p99 略升。

`tools/list` 仍在 IO 线程同步执行，但 Multi-Reactor 引入了连接分配、连接表 mutex、`shared_ptr` 等开销，B 场景 QPS 降约 **10%**。

### 2. 本次压测未覆盖升级的核心收益

Multi-Reactor + ThreadPool 的设计目标不是提升 `read_file` / `plugin_demo` 的绝对 QPS，而是：

- **重工具（shell fork）不阻塞 IO 线程**
- **多 Worker 分散连接 IO**
- **高并发混合负载下保护 `tools/list` 延迟**

当前套件全是轻量工具、各场景独立压测，**无法体现**「shell 阻塞时 list 是否仍能快速响应」这一核心优势。

### 3. 同机 4C VM 的资源竞争

`io_threads: 4` + `worker_threads: 4` + Main 线程 + 压测客户端线程，在 4 核 VM 上接近 CPU 饱和，Multi-Reactor 线程数多于单 Reactor，上下文切换开销更大。

### 4. 建议的后续压测

| 场景 | 目的 |
|------|------|
| 混合负载：50 连接压 shell + 10 连接压 tools/list | 验证 IO 不被阻塞 |
| 固定总请求数（各 5000） | 消除场景间总时长差异 |
| `reactor.io_threads` 扫参 1/2/4/8 | 找本机最优 IO 线程数 |
| 物理机 / 更多核心 | 减少 VM 噪声 |

## 复现

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j$(nproc)

./build-release/solarmcpd --config config.yaml

python3 scripts/load_test.py --suite \
  --output docs/benchmarks/$(date +%F)-load-test-multi-reactor-release.md
```

## 简要结论

| 维度 | 单 Reactor Release | Multi-Reactor Release |
|------|-------------------|----------------------|
| 轻量 RPC 绝对 QPS | **更高**（~4200–4800） | 略低（~3900–4500） |
| 重工具隔离 | 无（IO 线程阻塞） | **有**（ThreadPool 异步） |
| IO 并行度 | 单 epoll 线程 | **4 Worker 分散连接** |
| 适用场景 | 低并发、轻工具 | 生产、混合负载、shell/插件 |

**结论**：在纯轻量工具压测下，Multi-Reactor 版本 QPS 略低于单 Reactor Release（约 3–14%），属架构换隔离性的正常 trade-off；要验证升级价值，需补混合负载（shell + list 并发）压测。

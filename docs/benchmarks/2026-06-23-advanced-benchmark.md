# SolarMcp 高级压测报告

- 时间: 2026-06-23 07:37:56 CST
- 二进制: `build-release/solarmcpd`（Multi-Reactor + ThreadPool）
- 机器: VMware VM, 4 CPU, x86_64

原始数据：[`2026-06-23-advanced-benchmark.json`](2026-06-23-advanced-benchmark.json)

---

## 1. 混合负载（50 shell + 10 list）

背景：50 连接各 100 次 `sleep 0.05`（shell），与 10 连接各 500 次 `tools/list` **并发**执行，各 5000 请求。

| 架构 | 总耗时 | list QPS | list p50 | **list p99** | list max | shell QPS |
|------|--------|----------|----------|--------------|----------|-----------|
| Multi-Reactor (io=4) | 97.3s | 51.4 | 1.72ms | **9.06ms** | 15.6ms | 51.4 |
| Single-Reactor | 98.0s | 51.0 | 1.73ms | **6.98ms** | 11.8ms | 51.0 |

### 结论

- **IO 未被 shell 拖死**：两种架构下 `tools/list` p99 均 **< 10ms**，max < 16ms；shell 吞吐 ~51 QPS（受 4 业务线程 + `sleep 0.05` 限制）。
- Multi vs Single 差距很小：Phase 2 已将 `tools/call` **异步化**，Single 的 shell 也不占 IO 线程，故混合场景下 Multi 未显著优于 Single。
- 本场景验证的是 **ThreadPool 隔离**有效，而非 Multi-Reactor 独有优势。

---

## 2. 固定总请求数 5000

消除各场景总请求数差异，便于横向对比 QPS / p99。

| 场景 | 连接×请求 | QPS | p50 (ms) | p99 (ms) | 失败 |
|------|-----------|-----|----------|----------|------|
| F1. tools/list (50×100) | 50×100 | 4271 | 5.66 | 43.57 | 0 |
| F2. tools/list (10×500) | 10×500 | 4446 | 1.75 | **8.42** | 0 |
| F3. read_file (50×100) | 50×100 | 4198 | 5.54 | 37.90 | 0 |
| F4. plugin_demo (50×100) | 50×100 | 4338 | 5.48 | 36.87 | 0 |
| F5. shell echo (10×500) | 10×500 | **134** | 71.0 | 218.1 | 0 |

### 结论

- 轻量 RPC（list / read_file / plugin_demo）在 5000 请求下 QPS **4200–4450**。
- **50 连接**时 p99 明显高于 **10 连接**（F1 p99 43.6ms vs F2 8.4ms）——连接数对 tail latency 影响大于总请求数。
- shell（echo）QPS ~134，p99 ~218ms，符合 fork + 线程池排队预期。

---

## 3. io_threads 扫参（tools/list 5000, 50 连接）

固定 `reactor.mode: multi`，仅变 `io_threads`，每轮重启服务。

| io_threads | QPS | p50 (ms) | p99 (ms) | 失败 |
|------------|-----|----------|----------|------|
| **1** | **4606** | 5.43 | 39.79 | 0 |
| **2** | 4210 | 5.76 | **38.69** | 0 |
| 4 | 4322 | 5.54 | 40.92 | 0 |
| 8 | 4224 | 5.73 | 43.77 | 0 |

### 本机推荐

| 目标 | 推荐 `io_threads` |
|------|-------------------|
| 吞吐优先 | **1**（QPS 4606） |
| 延迟优先 | **2**（p99 38.7ms） |
| 当前默认 4 | 介于两者之间，非本机最优 |

4 核 VM 上 IO 线程过多（8）反而因上下文切换导致 p99 升至 43.8ms。

---

## 4. 与历史 Release 压测对比（固定 5000 口径）

| 场景 | 旧套件 (Multi) | 新套件 F1 (50×100) | 旧套件 B (50×100) |
|------|----------------|-------------------|-------------------|
| tools/list QPS | 4078 | **4271** | 4078 |
| tools/list p99 | 42.5ms | 43.6ms | 42.5ms |

同连接模型下结果一致，新套件总请求数统一为 5000，可与其他场景直接对比 QPS。

---

## 5. 配置建议（本机 4C VM）

```yaml
reactor:
  mode: "multi"
  io_threads: 2          # p99 最优；追求 QPS 可设 1

thread_pool:
  worker_threads: 4      # shell 并发上限，可按 CPU 调整
```

---

## 复现

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j$(nproc)

python3 scripts/benchmark_advanced.py \
  --output docs/benchmarks/$(date +%F)-advanced-benchmark.md
```

仅混合负载 / 跳过扫参：

```bash
python3 scripts/benchmark_advanced.py --skip-sweep
python3 scripts/benchmark_advanced.py --skip-mixed-single --skip-sweep  # 仅 mixed multi
```

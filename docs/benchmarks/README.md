# 压测存档

| 日期 | 架构 | 构建 | 报告 | JSON |
|------|------|------|------|------|
| 2026-06-23 | 单 Reactor | Debug | [2026-06-23-load-test.md](2026-06-23-load-test.md) | [2026-06-23-load-test.json](2026-06-23-load-test.json) |
| 2026-06-23 | 单 Reactor | Release | [2026-06-23-load-test-release.md](2026-06-23-load-test-release.md) | [2026-06-23-load-test-release.json](2026-06-23-load-test-release.json) |
| 2026-06-23 | **Multi-Reactor** | Release | [2026-06-23-load-test-multi-reactor-release.md](2026-06-23-load-test-multi-reactor-release.md) | [2026-06-23-load-test-multi-reactor-release.json](2026-06-23-load-test-multi-reactor-release.json) |
| 2026-06-23 | **高级**（混合/5000/io扫参） | Release | [2026-06-23-advanced-benchmark.md](2026-06-23-advanced-benchmark.md) | [2026-06-23-advanced-benchmark.json](2026-06-23-advanced-benchmark.json) |

## 高级压测要点

- **混合负载**：50×shell(`sleep 0.05`) + 10×list 并发 → list p99 **< 10ms**（IO 未被阻塞）
- **固定 5000 请求**：各场景总请求数统一，便于比 QPS
- **io_threads 扫参**：本机 4C VM 推荐 **1（QPS）** 或 **2（p99）**

运行高级压测：

```bash
python3 scripts/benchmark_advanced.py
```

| 场景 | 单 Reactor QPS | Multi-Reactor QPS | 单 p99 | Multi p99 |
|------|----------------|-------------------|--------|-----------|
| tools/list 基线 | 4671 | 4517 | 7.9ms | 7.7ms |
| tools/list 高并发 | 4520 | 4078 | 38.6ms | 42.5ms |
| read_file | 4269 | 3890 | 12.5ms | 14.8ms |
| plugin_demo | 4816 | 4131 | 13.5ms | 15.3ms |

运行压测：

```bash
python3 scripts/load_test.py --suite \
  --output docs/benchmarks/$(date +%F)-load-test-multi-reactor-release.md
```

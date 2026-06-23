# SolarMcp 压测报告（Release）

## 环境

| 项 | 值 |
|----|-----|
| 时间 | 2026-06-23 Release 复测 |
| 目标 | 127.0.0.1:8090 |
| 认证 | 启用 |
| Git commit | `c42cacb` |
| 构建 | **Release**（`-O2 -DNDEBUG`，`build-release/`） |
| 二进制 | `./build-release/solarmcpd` |
| 机器 | VMware VM, Linux 7.0.0-22-generic, x86_64, 4 CPU, 3.3Gi RAM |
| 脚本 | `python3 scripts/load_test.py --suite` |

原始 JSON：[`2026-06-23-load-test-release.json`](2026-06-23-load-test-release.json)

Debug 对照：[`2026-06-23-load-test.md`](2026-06-23-load-test.md)

## Debug vs Release 对比

| 场景 | Debug QPS | Release QPS | 提升 | Debug p99 | Release p99 |
|------|-----------|-------------|------|-----------|-------------|
| A. tools/list 基线 (10×200) | 416 | **4671** | **11.2×** | 31.5ms | 7.9ms |
| B. tools/list 高并发 (50×100) | 296 | **4520** | **15.3×** | 333.5ms | 38.6ms |
| C. read_file (20×50) | 840 | **4269** | **5.1×** | 29.9ms | 12.5ms |
| D. plugin_demo (20×50) | 1045 | **4816** | **4.6×** | 22.0ms | 13.5ms |

> Debug 构建含 `-O0 -g`，且未开 ASan 时仍显著慢于 Release；高并发场景差距最大（B 场景 15×）。

## Release 结果摘要

| 场景 | 连接×请求 | QPS | p50 (ms) | p99 (ms) | 失败 |
|------|-----------|-----|----------|----------|------|
| A. tools/list 基线 | 10×200 | 4670.6 | 1.7 | 7.9 | 0 |
| B. tools/list 高并发 | 50×100 | 4520.3 | 5.7 | 38.6 | 0 |
| C. read_file | 20×50 | 4268.5 | 3.0 | 12.5 | 0 |
| D. plugin_demo | 20×50 | 4815.6 | 2.5 | 13.5 | 0 |

## 详细数据

### A. tools/list 基线

- 成功: 2000/2000, 耗时: 0.428s, QPS: 4670.6
- 延迟 (ms): min=0.15, p50=1.72, p90=3.48, p99=7.93, max=13.82, mean=2.0

### B. tools/list 高并发

- 成功: 5000/5000, 耗时: 1.106s, QPS: 4520.3
- 延迟 (ms): min=0.14, p50=5.72, p90=19.0, p99=38.59, max=84.71, mean=8.33

### C. read_file 轻业务

- 成功: 1000/1000, 耗时: 0.234s, QPS: 4268.5
- 延迟 (ms): min=0.31, p50=2.98, p90=6.08, p99=12.47, max=15.78, mean=3.33

### D. plugin_demo 插件

- 成功: 1000/1000, 耗时: 0.208s, QPS: 4815.6
- 延迟 (ms): min=0.15, p50=2.52, p90=6.78, p99=13.46, max=20.06, mean=3.26

## 结论

1. Release 下四个场景 QPS 均在 **4200–4800** 区间，单 Reactor 在本机 VM 上可稳定处理约 **4500 req/s**（轻量 JSON-RPC）。
2. 50 连接高并发时 Release p99 仅 **38.6ms**（Debug 为 333ms），排队效应大幅缓解。
3. 压测生产环境请始终使用 Release 构建；Debug 数据仅适合功能调试，不宜作性能基线。

## 复现

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j$(nproc)

./build-release/solarmcpd --config config.yaml

python3 scripts/load_test.py --suite \
  --output docs/benchmarks/$(date +%F)-load-test-release.md
```

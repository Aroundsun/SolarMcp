#!/usr/bin/env python3
"""SolarMcp 高级压测：混合负载、固定总请求数、io_threads 扫参。"""

from __future__ import annotations

import argparse
import json
import re
import statistics
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from load_test import format_result, percentile, run_scenario
from mcp_tcp_client import McpClient, default_config_path, resolve_auth

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BINARY = PROJECT_ROOT / "build-release" / "solarmcpd"
BENCH_CONFIG = PROJECT_ROOT / "config.bench.yaml"


@dataclass
class ServerConfig:
    mode: str = "multi"
    io_threads: int = 4


def load_config_template() -> str:
    return (PROJECT_ROOT / "config.yaml").read_text(encoding="utf-8")


def write_bench_config(cfg: ServerConfig) -> Path:
    text = load_config_template()
    text = re.sub(
        r'mode:\s*"[^"]*"',
        f'mode: "{cfg.mode}"',
        text,
        count=1,
    )
    text = re.sub(
        r"io_threads:\s*\d+",
        f"io_threads: {cfg.io_threads}",
        text,
        count=1,
    )
    BENCH_CONFIG.write_text(text, encoding="utf-8")
    return BENCH_CONFIG


def stop_server() -> None:
    subprocess.run(["pkill", "-x", "solarmcpd"], check=False)
    time.sleep(1)


def wait_server(host: str, port: int, auth_enabled: bool, token: str,
                timeout_sec: float = 15.0) -> bool:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        try:
            client = McpClient(host, port)
            if auth_enabled:
                r = client.authenticate(token)
                if "error" in r:
                    client.close()
                    time.sleep(0.3)
                    continue
            client.close()
            return True
        except (ConnectionRefusedError, OSError):
            time.sleep(0.3)
    return False


def start_server(binary: Path, config_path: Path, host: str, port: int,
                 auth_enabled: bool, token: str) -> None:
    stop_server()
    subprocess.Popen(
        [str(binary), "--config", str(config_path)],
        cwd=str(PROJECT_ROOT),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if not wait_server(host, port, auth_enabled, token):
        raise RuntimeError("服务器未在预期时间内就绪")


def distribute_requests(total: int, connections: int) -> tuple[int, int]:
    """返回 (connections, requests_per_conn)，乘积 == total。"""
    if total % connections != 0:
        connections = _pick_connections(total, connections)
    return connections, total // connections


def _pick_connections(total: int, preferred: int) -> int:
    for c in (preferred, 50, 25, 20, 10, 5, 1):
        if total % c == 0:
            return c
    return 1


def run_mixed_load(
    *,
    name: str,
    host: str,
    port: int,
    auth_enabled: bool,
    token: str,
    shell_connections: int = 50,
    list_connections: int = 10,
    list_total: int = 5000,
    shell_command: str = "sleep 0.05",
    shell_requests_per_conn: int = 100,
) -> dict:
    """50 连接压 shell + 10 连接压 tools/list，并发执行。"""
    list_conn, list_req = distribute_requests(list_total, list_connections)
    shell_params = {
        "name": "shell",
        "arguments": {"command": shell_command},
    }

    def shell_worker() -> tuple[list[float], int]:
        lat: list[float] = []
        errors = 0
        client = McpClient(host, port)
        try:
            if auth_enabled:
                r = client.authenticate(token)
                if "error" in r:
                    return [], shell_requests_per_conn
            for _ in range(shell_requests_per_conn):
                t0 = time.perf_counter()
                resp = client.send_request("tools/call", shell_params)
                dt = (time.perf_counter() - t0) * 1000
                lat.append(dt)
                if "error" in resp:
                    errors += 1
        finally:
            client.close()
        return lat, errors

    def list_worker(requests: int) -> tuple[list[float], int]:
        lat: list[float] = []
        errors = 0
        client = McpClient(host, port)
        try:
            if auth_enabled:
                r = client.authenticate(token)
                if "error" in r:
                    return [], requests
            for _ in range(requests):
                t0 = time.perf_counter()
                resp = client.send_request("tools/list", {})
                dt = (time.perf_counter() - t0) * 1000
                lat.append(dt)
                if "error" in resp:
                    errors += 1
        finally:
            client.close()
        return lat, errors

    t0 = time.perf_counter()
    shell_lat: list[float] = []
    list_lat: list[float] = []
    shell_errors = 0
    list_errors = 0

    max_workers = shell_connections + list_conn
    with ThreadPoolExecutor(max_workers=max_workers) as ex:
        futs = []
        for _ in range(shell_connections):
            futs.append(("shell", ex.submit(shell_worker)))
        for _ in range(list_conn):
            futs.append(("list", ex.submit(list_worker, list_req)))

        for kind, f in futs:
            lat, err = f.result()
            if kind == "shell":
                shell_lat.extend(lat)
                shell_errors += err
            else:
                list_lat.extend(lat)
                list_errors += err

    elapsed = time.perf_counter() - t0
    list_lat.sort()
    shell_lat.sort()

    shell_total = shell_connections * shell_requests_per_conn
    list_total_actual = list_conn * list_req

    def lat_stats(data: list[float]) -> dict:
        if not data:
            return {}
        return {
            "min": round(data[0], 2),
            "p50": round(statistics.median(data), 2),
            "p90": round(percentile(data, 0.90), 2),
            "p99": round(percentile(data, 0.99), 2),
            "max": round(data[-1], 2),
            "mean": round(statistics.mean(data), 2),
        }

    list_ok = max(0, list_total_actual - list_errors)
    shell_ok = max(0, shell_total - shell_errors)

    return {
        "name": name,
        "type": "mixed",
        "shell": {
            "connections": shell_connections,
            "requests_per_conn": shell_requests_per_conn,
            "total_requests": shell_total,
            "command": shell_command,
            "success": shell_ok,
            "errors": shell_errors,
            "qps": round(shell_ok / elapsed, 1) if elapsed > 0 else 0,
            "latency_ms": lat_stats(shell_lat),
        },
        "tools_list": {
            "connections": list_conn,
            "requests_per_conn": list_req,
            "total_requests": list_total_actual,
            "success": list_ok,
            "errors": list_errors,
            "qps": round(list_ok / elapsed, 1) if elapsed > 0 else 0,
            "latency_ms": lat_stats(list_lat),
        },
        "elapsed_sec": round(elapsed, 3),
    }


def format_mixed(r: dict) -> str:
    sl = r["tools_list"]
    sh = r["shell"]
    lat = sl["latency_ms"]
    lines = [
        f"### {r['name']}",
        "",
        f"- 混合负载耗时: {r['elapsed_sec']}s",
        f"- **tools/list**（IO 验证）: {sl['connections']}×{sl['requests_per_conn']}="
        f"{sl['total_requests']}, QPS={sl['qps']}, 失败={sl['errors']}",
    ]
    if lat:
        lines.append(
            f"  - list 延迟 (ms): p50={lat['p50']}, p99={lat['p99']}, max={lat['max']}"
        )
    lines.append(
        f"- shell（背景负载）: {sh['connections']}×{sh['requests_per_conn']}="
        f"{sh['total_requests']}, cmd=`{sh['command']}`, QPS={sh['qps']}"
    )
    lines.append("")
    return "\n".join(lines)


def fixed_5000_suite(host: str, port: int, auth_enabled: bool, token: str,
                     path: str) -> list[dict]:
    total = 5000
    scenarios = [
        ("F1. tools/list (50×100)", "tools/list", {}, 50),
        ("F2. tools/list (10×500)", "tools/list", {}, 10),
        ("F3. read_file (50×100)", "tools/call",
         {"name": "read_file", "arguments": {"path": path}}, 50),
        ("F4. plugin_demo (50×100)", "tools/call",
         {"name": "plugin_demo", "arguments": {}}, 50),
        ("F5. shell echo (10×500)", "tools/call",
         {"name": "shell", "arguments": {"command": "echo ok"}}, 10),
    ]
    results = []
    for name, method, params, conn in scenarios:
        c, req = distribute_requests(total, conn)
        r = run_scenario(
            name=name,
            host=host,
            port=port,
            auth_enabled=auth_enabled,
            token=token,
            method=method,
            params=params,
            connections=c,
            requests=req,
        )
        r["total_requests_fixed"] = total
        results.append(r)
    return results


def io_threads_sweep(
    host: str,
    port: int,
    auth_enabled: bool,
    token: str,
    binary: Path,
    io_values: list[int],
) -> list[dict]:
    """对每个 io_threads 重启服务器并压 tools/list 5000（50 连接）。"""
    results = []
    for io in io_values:
        cfg = ServerConfig(mode="multi", io_threads=io)
        config_path = write_bench_config(cfg)
        print(f"\n>>> io_threads={io} 启动服务器...", file=sys.stderr)
        start_server(binary, config_path, host, port, auth_enabled, token)

        c, req = distribute_requests(5000, 50)
        r = run_scenario(
            name=f"Sweep io={io} tools/list 5000",
            host=host,
            port=port,
            auth_enabled=auth_enabled,
            token=token,
            method="tools/list",
            params={},
            connections=c,
            requests=req,
        )
        r["io_threads"] = io
        r["reactor_mode"] = "multi"
        results.append(r)
        print(format_result(r), file=sys.stderr)
    return results


def format_sweep_table(results: list[dict]) -> str:
    lines = [
        "| io_threads | QPS | p50 (ms) | p99 (ms) | 失败 |",
        "|------------|-----|----------|----------|------|",
    ]
    best_qps = max(results, key=lambda x: x["qps"])
    best_p99 = min(results, key=lambda x: x["latency_ms"].get("p99", 1e9))

    for r in results:
        lat = r.get("latency_ms", {})
        mark = ""
        if r["io_threads"] == best_qps["io_threads"]:
            mark = " ← QPS最高"
        elif r["io_threads"] == best_p99["io_threads"]:
            mark = " ← p99最低"
        lines.append(
            f"| {r['io_threads']} | {r['qps']} | {lat.get('p50', '-')} "
            f"| {lat.get('p99', '-')} | {r['errors']}{mark}|"
        )
    lines.append("")
    lines.append(
        f"**推荐**: QPS 最优 `io_threads={best_qps['io_threads']}`；"
        f"p99 最优 `io_threads={best_p99['io_threads']}`"
    )
    return "\n".join(lines)


def run_advanced(
    *,
    host: str,
    port: int,
    config_path: Path,
    binary: Path,
    output: Path | None,
    skip_sweep: bool = False,
    skip_mixed_single: bool = False,
) -> dict:
    auth_enabled, token = resolve_auth(config_path, None, False)
    report: dict = {
        "meta": {
            "time": time.strftime("%Y-%m-%d %H:%M:%S %Z"),
            "host": host,
            "port": port,
            "binary": str(binary),
        },
        "mixed_load": [],
        "fixed_5000": [],
        "io_sweep": [],
    }
    md_parts = [
        "# SolarMcp 高级压测报告",
        "",
        f"- 时间: {report['meta']['time']}",
        f"- 二进制: `{binary}`",
        "",
    ]

    if not binary.is_file():
        raise FileNotFoundError(f"未找到 Release 二进制: {binary}")

    # --- 混合负载：multi (io=4) ---
    cfg_multi = ServerConfig(mode="multi", io_threads=4)
    write_bench_config(cfg_multi)
    print(">>> 混合负载 Multi-Reactor (io=4)...", file=sys.stderr)
    start_server(binary, BENCH_CONFIG, host, port, auth_enabled, token)
    mixed_multi = run_mixed_load(
        name="混合负载 Multi-Reactor (io=4)",
        host=host, port=port,
        auth_enabled=auth_enabled, token=token,
    )
    mixed_multi["reactor_mode"] = "multi"
    mixed_multi["io_threads"] = 4
    report["mixed_load"].append(mixed_multi)

    # --- 混合负载：single 基线 ---
    if not skip_mixed_single:
        cfg_single = ServerConfig(mode="single", io_threads=0)
        write_bench_config(cfg_single)
        print(">>> 混合负载 Single-Reactor 基线...", file=sys.stderr)
        start_server(binary, BENCH_CONFIG, host, port, auth_enabled, token)
        mixed_single = run_mixed_load(
            name="混合负载 Single-Reactor",
            host=host, port=port,
            auth_enabled=auth_enabled, token=token,
        )
        mixed_single["reactor_mode"] = "single"
        mixed_single["io_threads"] = 0
        report["mixed_load"].append(mixed_single)

    md_parts.extend(["## 1. 混合负载（50 shell + 10 list）", ""])
    for m in report["mixed_load"]:
        md_parts.append(format_mixed(m))

    if len(report["mixed_load"]) == 2:
        mm = report["mixed_load"][0]["tools_list"]["latency_ms"]
        ms = report["mixed_load"][1]["tools_list"]["latency_ms"]
        if mm and ms:
            md_parts.append(
                f"**IO 隔离对比**: Multi list p99={mm['p99']}ms vs "
                f"Single list p99={ms['p99']}ms "
                f"({'Multi 更优' if mm['p99'] < ms['p99'] else 'Single 更优'})"
            )
            md_parts.append("")

    # --- 固定 5000 请求（multi io=4）---
    write_bench_config(cfg_multi)
    start_server(binary, BENCH_CONFIG, host, port, auth_enabled, token)
    print(">>> 固定 5000 请求套件...", file=sys.stderr)
    fixed = fixed_5000_suite(host, port, auth_enabled, token, "/tmp/test.txt")
    report["fixed_5000"] = fixed

    md_parts.extend(["## 2. 固定总请求数 5000", ""])
    md_parts.append("| 场景 | 连接×请求 | QPS | p99 (ms) | 失败 |")
    md_parts.append("|------|-----------|-----|----------|------|")
    for r in fixed:
        lat = r.get("latency_ms", {})
        md_parts.append(
            f"| {r['name']} | {r['connections']}×{r['requests_per_conn']} "
            f"| {r['qps']} | {lat.get('p99', '-')} | {r['errors']} |"
        )
    md_parts.append("")

    # --- io_threads 扫参 ---
    if not skip_sweep:
        print(">>> io_threads 扫参 1/2/4/8...", file=sys.stderr)
        sweep = io_threads_sweep(
            host, port, auth_enabled, token, binary, [1, 2, 4, 8],
        )
        report["io_sweep"] = sweep
        md_parts.extend(["## 3. io_threads 扫参（tools/list 5000, 50 连接）", ""])
        md_parts.append(format_sweep_table(sweep))

    if output:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text("\n".join(md_parts), encoding="utf-8")
        output.with_suffix(".json").write_text(
            json.dumps(report, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
        print(f"\n报告: {output}")
        print(f"JSON: {output.with_suffix('.json')}")

    return report


def main() -> None:
    p = argparse.ArgumentParser(description="SolarMcp 高级压测")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=8090)
    p.add_argument("--config", type=Path, default=default_config_path())
    p.add_argument("--binary", type=Path, default=DEFAULT_BINARY)
    p.add_argument("--output", type=Path,
                   default=PROJECT_ROOT / "docs/benchmarks/2026-06-23-advanced-benchmark.md")
    p.add_argument("--skip-sweep", action="store_true")
    p.add_argument("--skip-mixed-single", action="store_true")
    args = p.parse_args()

    run_advanced(
        host=args.host,
        port=args.port,
        config_path=args.config,
        binary=args.binary,
        output=args.output,
        skip_sweep=args.skip_sweep,
        skip_mixed_single=args.skip_mixed_single,
    )


if __name__ == "__main__":
    main()

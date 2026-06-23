#!/usr/bin/env python3
"""SolarMcp 简易压测：多连接 × 每连接多请求。"""

from __future__ import annotations

import argparse
import json
import statistics
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mcp_tcp_client import McpClient, default_config_path, resolve_auth


def one_worker(
    host: str,
    port: int,
    auth_enabled: bool,
    token: str,
    method: str,
    params: dict,
    requests_per_conn: int,
) -> tuple[list[float], int]:
    latencies: list[float] = []
    errors = 0
    client = McpClient(host, port)
    try:
        if auth_enabled:
            r = client.authenticate(token)
            if "error" in r:
                return [], requests_per_conn

        for _ in range(requests_per_conn):
            t0 = time.perf_counter()
            resp = client.send_request(method, params)
            dt = (time.perf_counter() - t0) * 1000
            latencies.append(dt)
            if "error" in resp:
                errors += 1
    finally:
        client.close()
    return latencies, errors


def percentile(sorted_lat: list[float], p: float) -> float:
    if not sorted_lat:
        return 0.0
    idx = max(0, min(len(sorted_lat) - 1, int(len(sorted_lat) * p) - 1))
    return sorted_lat[idx]


def run_scenario(
    *,
    name: str,
    host: str,
    port: int,
    auth_enabled: bool,
    token: str,
    method: str,
    params: dict,
    connections: int,
    requests: int,
) -> dict:
    total = connections * requests
    t0 = time.perf_counter()

    all_lat: list[float] = []
    total_errors = 0
    with ThreadPoolExecutor(max_workers=connections) as ex:
        futs = [
            ex.submit(
                one_worker,
                host,
                port,
                auth_enabled,
                token,
                method,
                params,
                requests,
            )
            for _ in range(connections)
        ]
        for f in as_completed(futs):
            lat, err = f.result()
            all_lat.extend(lat)
            total_errors += err

    elapsed = time.perf_counter() - t0
    ok = max(0, total - total_errors)
    all_lat.sort()

    result = {
        "name": name,
        "method": method,
        "params": params,
        "connections": connections,
        "requests_per_conn": requests,
        "total_requests": total,
        "success": ok,
        "errors": total_errors,
        "elapsed_sec": round(elapsed, 3),
        "qps": round(ok / elapsed, 1) if elapsed > 0 else 0,
        "latency_ms": {},
    }
    if all_lat:
        result["latency_ms"] = {
            "min": round(all_lat[0], 2),
            "p50": round(statistics.median(all_lat), 2),
            "p90": round(percentile(all_lat, 0.90), 2),
            "p99": round(percentile(all_lat, 0.99), 2),
            "max": round(all_lat[-1], 2),
            "mean": round(statistics.mean(all_lat), 2),
        }
    return result


def format_result(r: dict) -> str:
    lines = [
        f"### {r['name']}",
        "",
        f"- 方法: `{r['method']}`",
        f"- 连接数: {r['connections']}, 每连接请求: {r['requests_per_conn']}, 总请求: {r['total_requests']}",
        f"- 成功: {r['success']}, 失败: {r['errors']}, 耗时: {r['elapsed_sec']}s",
        f"- QPS: {r['qps']}",
    ]
    if r["latency_ms"]:
        lat = r["latency_ms"]
        lines.append(
            f"- 延迟 (ms): min={lat['min']}, p50={lat['p50']}, "
            f"p90={lat['p90']}, p99={lat['p99']}, max={lat['max']}, mean={lat['mean']}"
        )
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    p = argparse.ArgumentParser(description="SolarMcp 压测")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=8090)
    p.add_argument("--connections", "-c", type=int, default=10)
    p.add_argument("--requests", "-n", type=int, default=100, help="每连接请求数")
    p.add_argument("--method", default="tools/list")
    p.add_argument("--tool", default="read_file")
    p.add_argument("--path", default="/tmp/test.txt")
    p.add_argument("--name", default="", help="场景名称（用于报告）")
    p.add_argument("--config", type=Path, default=default_config_path())
    p.add_argument("--no-auth", action="store_true")
    p.add_argument("--json", action="store_true", help="输出 JSON 结果")
    p.add_argument("--suite", action="store_true", help="运行预设压测套件")
    p.add_argument("--output", type=Path, help="将结果写入文件（markdown + json）")
    args = p.parse_args()

    auth_enabled, token = resolve_auth(args.config, None, args.no_auth)

    if args.suite:
        scenarios = [
            ("A. tools/list 基线", "tools/list", {}, 10, 200),
            ("B. tools/list 高并发", "tools/list", {}, 50, 100),
            ("C. read_file 轻业务", "tools/call",
             {"name": "read_file", "arguments": {"path": args.path}}, 20, 50),
            ("D. plugin_demo 插件", "tools/call",
             {"name": "plugin_demo", "arguments": {}}, 20, 50),
        ]
        results = []
        for name, method, params, conn, req in scenarios:
            r = run_scenario(
                name=name,
                host=args.host,
                port=args.port,
                auth_enabled=auth_enabled,
                token=token,
                method=method,
                params=params,
                connections=conn,
                requests=req,
            )
            results.append(r)
            if not args.json:
                print(format_result(r))

        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            md_path = args.output
            json_path = args.output.with_suffix(".json")
            md_lines = [
                "# SolarMcp 压测报告",
                "",
                f"- 时间: {time.strftime('%Y-%m-%d %H:%M:%S %Z')}",
                f"- 目标: {args.host}:{args.port}",
                f"- 认证: {'启用' if auth_enabled else '关闭'}",
                "",
            ]
            for r in results:
                md_lines.append(format_result(r))
            md_path.write_text("\n".join(md_lines), encoding="utf-8")
            json_path.write_text(
                json.dumps(results, indent=2, ensure_ascii=False),
                encoding="utf-8",
            )
            print(f"报告已写入: {md_path}")
            print(f"JSON 已写入: {json_path}")
        elif args.json:
            print(json.dumps(results, indent=2, ensure_ascii=False))
        return

    if args.method == "tools/call":
        params = {"name": args.tool, "arguments": {"path": args.path} if args.tool == "read_file" else {}}
    else:
        params = {}

    name = args.name or f"{args.method} c={args.connections} n={args.requests}"
    r = run_scenario(
        name=name,
        host=args.host,
        port=args.port,
        auth_enabled=auth_enabled,
        token=token,
        method=args.method,
        params=params,
        connections=args.connections,
        requests=args.requests,
    )

    if args.json:
        print(json.dumps(r, indent=2, ensure_ascii=False))
    else:
        print(format_result(r))

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(format_result(r), encoding="utf-8")
        args.output.with_suffix(".json").write_text(
            json.dumps(r, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )


if __name__ == "__main__":
    main()

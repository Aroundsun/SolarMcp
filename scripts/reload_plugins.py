#!/usr/bin/env python3
"""SolarMcp 插件热重载脚本。

在服务器运行中调用 plugins/reload，无需重启 solarmcpd。

用法:
    cmake --build build --target shell_plugin
    python3 scripts/reload_plugins.py
    python3 scripts/reload_plugins.py --list-tools
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# 允许从项目根目录以 python3 scripts/reload_plugins.py 运行
sys.path.insert(0, str(Path(__file__).resolve().parent))

from mcp_tcp_client import (
    authenticate_or_exit,
    connect_or_exit,
    default_config_path,
    print_response,
    resolve_auth,
    tool_names,
)


def print_reload_summary(response: dict) -> None:
    result = response.get("result", {})
    if not result:
        return

    print()
    print("--- 热重载摘要 ---")
    print(f"  卸载插件: {result.get('unloaded', '?')}")
    print(f"  加载成功: {result.get('loaded', '?')}")
    print(f"  加载失败: {result.get('failed', '?')}")
    print(f"  当前工具: {result.get('tools', '?')}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="SolarMcp 插件热重载（plugins/reload）",
        epilog="详见 docs/插件热重载.md",
    )
    parser.add_argument("--host", default="127.0.0.1", help="服务器地址")
    parser.add_argument("--port", type=int, default=8090, help="服务器端口")
    parser.add_argument("--config", type=default_config_path,
                        help="配置文件路径（读取 auth 设置）")
    parser.add_argument("--token", help="认证 Token（默认从 config.yaml 读取）")
    parser.add_argument("--no-auth", action="store_true",
                        help="跳过认证（服务器未启用 auth 时使用）")
    parser.add_argument("--list-tools", action="store_true",
                        help="热重载后打印 tools/list")
    parser.add_argument("--quiet", "-q", action="store_true",
                        help="仅输出摘要，不打印完整 JSON")
    args = parser.parse_args()

    auth_enabled, token = resolve_auth(args.config, args.token, args.no_auth)

    client = connect_or_exit(args.host, args.port)
    try:
        authenticate_or_exit(client, auth_enabled, token, verbose=not args.quiet)

        resp = client.send_request("plugins/reload", {})
        if "error" in resp:
            if not args.quiet:
                print_response("plugins/reload", resp)
            else:
                err = resp["error"]
                print(f"错误: [{err.get('code')}] {err.get('message')}",
                      file=sys.stderr)
            return 1

        if args.quiet:
            print_reload_summary(resp)
        else:
            print_response("plugins/reload", resp)
            print_reload_summary(resp)

        if args.list_tools:
            list_resp = client.send_request("tools/list", {})
            if "error" in list_resp:
                print_response("tools/list", list_resp)
                return 1

            if args.quiet:
                names = tool_names(list_resp)
                print(f"  工具列表: {', '.join(names) if names else '(空)'}")
            else:
                print()
                print_response("tools/list", list_resp)
                names = tool_names(list_resp)
                print(f"\n工具名: {', '.join(names) if names else '(空)'}")

        return 0
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())

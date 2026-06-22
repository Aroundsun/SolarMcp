#!/usr/bin/env python3
"""SolarMcp TCP 客户端测试脚本（支持 Token 认证）。"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from mcp_tcp_client import (
    authenticate_or_exit,
    connect_or_exit,
    default_config_path,
    print_response,
    resolve_auth,
    tool_names,
)


def main() -> int:
    parser = argparse.ArgumentParser(description="SolarMcp 客户端测试")
    parser.add_argument("--host", default="127.0.0.1", help="服务器地址")
    parser.add_argument("--port", type=int, default=8090, help="服务器端口")
    parser.add_argument("--config", type=Path, default=default_config_path(),
                        help="配置文件路径（读取 auth 设置，默认: config.yaml）")
    parser.add_argument("--token", help="认证 Token（默认从 config.yaml 读取）")
    parser.add_argument("--no-auth", action="store_true",
                        help="跳过认证（服务器未启用 auth 时使用）")
    parser.add_argument("--shell-command", default="echo hello SolarMcp",
                        help="shell 工具要执行的命令（默认: echo hello SolarMcp）")
    parser.add_argument("--skip-shell", action="store_true",
                        help="跳过 shell 工具测试")
    args = parser.parse_args()

    auth_enabled, token = resolve_auth(args.config, args.token, args.no_auth)

    client = connect_or_exit(args.host, args.port)
    try:
        authenticate_or_exit(client, auth_enabled, token)

        resp = client.send_request("tools/list", {})
        print()
        print_response("tools/list", resp)
        if "error" in resp:
            return 1

        available = set(tool_names(resp))

        resp = client.send_request("tools/call", {
            "name": "read_file",
            "arguments": {"path": "/tmp/test.txt"},
        })
        print()
        print_response("tools/call read_file", resp)
        if "error" in resp:
            return 1

        if args.skip_shell:
            return 0

        if "shell" not in available:
            print("\n警告: tools/list 中未找到 shell 工具，跳过 shell 测试",
                  file=sys.stderr)
            print("提示: 确认已编译且 plugins/shell/shell_plugin.so 存在，"
                  "plugins/shell/shell.yaml 中 enabled=true",
                  file=sys.stderr)
            return 0

        resp = client.send_request("tools/call", {
            "name": "shell",
            "arguments": {"command": args.shell_command},
        })
        print()
        print_response("tools/call shell", resp)
        if "error" in resp:
            return 1

        result = resp.get("result", {})
        if result.get("status") != "success":
            print(f"警告: shell 命令非零退出，exit_code={result.get('exit_code')}",
                  file=sys.stderr)

        return 0
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())

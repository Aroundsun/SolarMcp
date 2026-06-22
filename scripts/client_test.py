#!/usr/bin/env python3
"""SolarMcp TCP 客户端测试脚本（支持 Token 认证）。"""

import argparse
import json
import socket
import sys
from pathlib import Path


def load_auth_config(config_path: Path) -> tuple[bool, str]:
    """从 config.yaml 读取 auth.enabled 与 auth.token（简单解析）。"""
    enabled = False
    token = ""
    in_auth = False

    for line in config_path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped == "auth:":
            in_auth = True
            continue
        if in_auth and not line.startswith((" ", "\t")):
            break
        if not in_auth:
            continue

        if stripped.startswith("enabled:"):
            value = stripped.split(":", 1)[1].strip().lower()
            enabled = value in ("true", "yes", "1")
        elif stripped.startswith("token:"):
            value = stripped.split(":", 1)[1].strip()
            if (value.startswith('"') and value.endswith('"')) or (
                value.startswith("'") and value.endswith("'")
            ):
                value = value[1:-1]
            token = value

    return enabled, token


class McpClient:
    """在同一 TCP 连接上发送 JSON-RPC 请求（认证状态绑定于连接）。"""

    def __init__(self, host: str, port: int):
        self.sock = socket.create_connection((host, port))
        self._req_id = 0

    def send_request(self, method: str, params: dict) -> dict:
        self._req_id += 1
        body = json.dumps({
            "jsonrpc": "2.0",
            "method": method,
            "params": params,
            "id": self._req_id,
        })
        frame = f"Content-Length: {len(body.encode())}\r\n\r\n{body}"
        self.sock.sendall(frame.encode())

        data = b""
        while b"\r\n\r\n" not in data:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("服务器关闭了连接")
            data += chunk

        header, rest = data.split(b"\r\n\r\n", 1)
        length = int(header.decode().split(": ", 1)[1])
        while len(rest) < length:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("响应不完整")
            rest += chunk

        return json.loads(rest[:length].decode())

    def authenticate(self, token: str) -> dict:
        return self.send_request("authenticate", {"token": token})

    def close(self) -> None:
        self.sock.close()


def print_response(title: str, response: dict) -> None:
    print(f"=== {title} ===")
    print(json.dumps(response, indent=2, ensure_ascii=False))


def main() -> int:
    project_root = Path(__file__).resolve().parent.parent
    default_config = project_root / "config.yaml"

    parser = argparse.ArgumentParser(description="SolarMcp 客户端测试")
    parser.add_argument("--host", default="127.0.0.1", help="服务器地址")
    parser.add_argument("--port", type=int, default=8090, help="服务器端口")
    parser.add_argument("--config", type=Path, default=default_config,
                        help="配置文件路径（读取 auth 设置）")
    parser.add_argument("--token", help="认证 Token（默认从 config.yaml 读取）")
    parser.add_argument("--no-auth", action="store_true",
                        help="跳过认证（服务器未启用 auth 时使用）")
    args = parser.parse_args()

    auth_enabled = False
    token = args.token or ""

    if args.config.is_file():
        auth_enabled, config_token = load_auth_config(args.config)
        if not token:
            token = config_token
    elif not args.no_auth:
        print(f"警告: 未找到配置文件 {args.config}，假定未启用认证", file=sys.stderr)

    if args.no_auth:
        auth_enabled = False

    try:
        client = McpClient(args.host, args.port)
    except ConnectionRefusedError:
        print("错误: 无法连接服务器，请先启动 ./build/solarmcpd --config config.yaml",
              file=sys.stderr)
        return 1

    try:
        if auth_enabled:
            if not token:
                print("错误: auth.enabled=true 但未配置 token", file=sys.stderr)
                return 1

            resp = client.authenticate(token)
            print_response("authenticate", resp)
            if "error" in resp:
                return 1

        resp = client.send_request("tools/list", {})
        print()
        print_response("tools/list", resp)
        if "error" in resp:
            return 1

        resp = client.send_request("tools/call", {
            "name": "read_file",
            "arguments": {"path": "/tmp/test.txt"},
        })
        print()
        print_response("tools/call read_file", resp)
        if "error" in resp:
            return 1

        return 0
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())

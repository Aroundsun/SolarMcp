#!/usr/bin/env python3
"""SolarMcp TCP 客户端公共模块。"""

from __future__ import annotations

import json
import socket
import sys
from pathlib import Path


def project_root() -> Path:
    return Path(__file__).resolve().parent.parent


def default_config_path() -> Path:
    return project_root() / "config.yaml"


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


def resolve_auth(
    config_path: Path | None,
    token_override: str | None,
    no_auth: bool,
) -> tuple[bool, str]:
    """根据配置与命令行参数解析是否认证及 token。"""
    auth_enabled = False
    token = token_override or ""

    if config_path is None:
        config_path = default_config_path()

    if config_path.is_file():
        auth_enabled, config_token = load_auth_config(config_path)
        if not token:
            token = config_token
    elif not no_auth:
        print(f"警告: 未找到配置文件 {config_path}，假定未启用认证",
              file=sys.stderr)

    if no_auth:
        auth_enabled = False

    return auth_enabled, token


class McpClient:
    """在同一 TCP 连接上发送 JSON-RPC 请求（认证状态绑定于连接）。"""

    def __init__(self, host: str, port: int):
        self.sock = socket.create_connection((host, port))
        self._req_id = 0

    def send_request(self, method: str, params: dict | None = None) -> dict:
        self._req_id += 1
        body = json.dumps({
            "jsonrpc": "2.0",
            "method": method,
            "params": params or {},
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


def tool_names(tools_list_response: dict) -> list[str]:
    """从 tools/list 响应中提取工具名列表。"""
    result = tools_list_response.get("result", {})
    tools = result.get("tools", [])
    return sorted(
        t.get("name", "")
        for t in tools
        if isinstance(t, dict) and t.get("name")
    )


def connect_or_exit(host: str, port: int) -> McpClient:
    try:
        return McpClient(host, port)
    except ConnectionRefusedError:
        print("错误: 无法连接服务器，请先启动 ./build/solarmcpd --config config.yaml",
              file=sys.stderr)
        raise SystemExit(1) from None


def authenticate_or_exit(client: McpClient, auth_enabled: bool, token: str,
                         *, verbose: bool = True) -> None:
    if not auth_enabled:
        return
    if not token:
        print("错误: auth.enabled=true 但未配置 token", file=sys.stderr)
        raise SystemExit(1)

    resp = client.authenticate(token)
    if verbose:
        print_response("authenticate", resp)
    if "error" in resp:
        raise SystemExit(1)

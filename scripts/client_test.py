import json
import socket


def send_request(method, params, req_id=1):
    body = json.dumps({
        "jsonrpc": "2.0",
        "method": method,
        "params": params,
        "id": req_id,
    })
    frame = f"Content-Length: {len(body.encode())}\r\n\r\n{body}"

    sock = socket.create_connection(("127.0.0.1", 8090))
    sock.sendall(frame.encode())

    data = b""
    while b"\r\n\r\n" not in data:
        data += sock.recv(4096)
    header, rest = data.split(b"\r\n\r\n", 1)
    length = int(header.decode().split(": ", 1)[1])
    while len(rest) < length:
        rest += sock.recv(4096)

    print(json.dumps(json.loads(rest[:length].decode()), indent=2, ensure_ascii=False))
    sock.close()


if __name__ == "__main__":
    # 1. 列出工具
    print("=== tools/list ===")
    send_request("tools/list", {})

    # 2. 调用 read_file（路径需在 config.yaml 的 allowed_paths 内）
    print("\n=== tools/call read_file ===")
    send_request("tools/call", {
        "name": "read_file",
        "arguments": {"path": "/tmp/test.txt"},
    })

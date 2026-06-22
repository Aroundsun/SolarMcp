# SolarMcp

从零实现的高性能 **C++20 MCP（Model Context Protocol）** 服务器框架。

## 特性

- **Reactor 模式**：基于 epoll 的事件循环，边缘触发 I/O
- **线程池**：固定大小工作线程池，支持基于 `std::future` 的任务提交
- **异步日志**：双缓冲、非阻塞写入
- **JSON-RPC 2.0**：完整协议支持，Content-Length 分帧
- **插件系统**：基于 dlopen 的动态工具加载，[纯 C ABI](docs/插件ABI.md)（`plugin_abi.h`），支持 [热重载](docs/插件热重载.md)
- **MCP 工具**：可扩展工具注册表，支持 `tools/list` 与 `tools/call`
- **全面 RAII**：现代 C++ 内存管理，避免裸指针泄漏

## 环境要求

- **编译器**：GCC 13+ 或 Clang 17+（需支持 C++20）
- **构建系统**：CMake ≥ 3.20
- **操作系统**：Linux（epoll、dlopen、pthread）
- **依赖**：通过 CMake FetchContent 自动拉取

## 快速开始

```bash
# 配置并编译
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# 启动服务器（需先创建日志目录）
mkdir -p logs
./build/solarmcpd --config config.yaml

# 运行单元测试
ctest --test-dir build --output-on-failure
```

或使用构建脚本：

```bash
./scripts/build.sh Debug
```

### 测试客户端

服务器启动后，**另开终端**运行：

```bash
echo "hello SolarMcp" > /tmp/test.txt
python3 scripts/client_test.py
```

成功时会分别打印 `authenticate`（若启用认证）、`tools/list`、`tools/call read_file` 与 `tools/call shell` 的 JSON 响应。

自定义 shell 命令：

```bash
python3 scripts/client_test.py --shell-command "uname -a"
python3 scripts/client_test.py --skip-shell   # 仅测 read_file
```

`shell` 工具由 `plugins/lib/shell_plugin.so` 提供（编译后自动生成）。调用示例：

```bash
# tools/call 参数示例
{"name": "shell", "arguments": {"command": "echo hello"}}
```

> **安全提示**：`shell` 等同于远程命令执行，生产环境请保持 `auth.enabled: true` 并更换默认 token。

### 插件热重载

改插件代码后无需重启服务器：

```bash
cmake --build build --target shell_plugin
python3 scripts/reload_plugins.py --list-tools
```

详见 **[docs/插件热重载.md](docs/插件热重载.md)**；编写插件见 **[docs/插件ABI.md](docs/插件ABI.md)**。

> **说明**：本项目当前通过 **TCP（默认 8090 端口）** 提供 JSON-RPC 服务，不是 Cursor 常见的 stdio MCP。若端口已被占用，先结束旧进程再启动：`ss -ltnp | grep 8090`，然后 `kill <PID>`。

## 配置

编辑 `config.yaml` 自定义服务器设置：

```yaml
server:
  host: "0.0.0.0"
  port: 8090

logging:
  level: "info"
  file: "./logs/solarmcp.log"

plugins:
  directory: "./plugins/lib/"   # 动态库目录（源码在 plugins/ 各子目录）
  autoload: true
  allow_reload: true            # 允许 plugins/reload 热重载

tools:
  read_file:
    enabled: true
    max_size_mb: 10
    allowed_paths: ["/tmp", "/home"]

  shell:
    enabled: true
    timeout_sec: 30
    max_output_mb: 10
    allowed_shells: ["/bin/sh", "/bin/bash"]
```

## 项目结构

```
SolarMcp/
├── app/              # 程序入口
├── include/mcp/      # 公开头文件
│   ├── common/       # 公共类型与工具
│   ├── config/       # 配置管理
│   ├── logger/       # 异步日志系统
│   ├── network/      # 网络原语（Buffer、Socket）
│   ├── plugin/       # 插件管理
│   ├── protocol/     # JSON-RPC 2.0 编解码
│   ├── reactor/      # 事件循环（epoll）
│   ├── server/       # TCP 服务器与分发器
│   ├── thread/       # 线程池
│   ├── timer/        # 时间轮
│   └── tool/         # 工具抽象与实现
├── src/              # 实现源码
├── plugins/          # 插件源码与动态库
│   ├── example/      #   最小示例插件
│   ├── shell/        #   shell 插件源码
│   └── lib/          #   运行时 .so（构建生成）
├── docs/             # 文档
│   ├── 插件ABI.md        # 纯 C 插件 ABI 设计与编写
│   ├── 插件热重载.md     # 运行时热重载指南
│   └── 系统设计.md       # 架构与任务分解
├── scripts/          # 构建与测试脚本
├── tests/            # GoogleTest 单元测试
└── third_party/      # FetchContent 依赖管理
```

## 许可证

MIT

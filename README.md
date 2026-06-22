# SolarMcp

从零实现的高性能 **C++20 MCP（Model Context Protocol）** 服务器框架。

## 特性

- **Reactor 模式**：基于 epoll 的事件循环，边缘触发 I/O
- **线程池**：固定大小工作线程池，支持基于 `std::future` 的任务提交
- **异步日志**：双缓冲、非阻塞写入
- **JSON-RPC 2.0**：完整协议支持，Content-Length 分帧
- **插件系统**：基于 dlopen 的动态工具加载，C ABI 接口
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

成功时会分别打印 `tools/list` 与 `tools/call read_file` 的 JSON 响应。

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
  directory: "./plugins/"
  autoload: true

tools:
  read_file:
    enabled: true
    max_size_mb: 10
    allowed_paths: ["/tmp", "/home"]
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
├── plugins/          # 动态加载插件
├── scripts/          # 构建与测试脚本
├── tests/            # GoogleTest 单元测试
└── third_party/      # FetchContent 依赖管理
```

## 许可证

MIT

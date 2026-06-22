# 插件目录说明

本目录存放插件**源码**与**构建配置**；编译生成的动态库（`.so`）输出到 `lib/` 子目录。

## 目录结构

```
plugins/
├── CMakeLists.txt          # 插件构建规则
├── README.md               # 本文件
├── example/                # 最小示例插件（演示 C ABI，无业务逻辑）
│   └── minimal_plugin.cpp
├── shell/                  # shell 工具插件源码
│   ├── shell_tool.h
│   ├── shell_tool.cpp
│   └── shell_plugin.cpp
└── lib/                    # 运行时加载的 .so（构建后生成，已 gitignore）
    ├── minimal_plugin.so
    └── shell_plugin.so
```

> **说明**：`read_file` 为**内置工具**（见 `app/main.cpp`），不再提供同名插件，避免重复注册。

## 构建与加载

```bash
# 编译全部插件，.so 自动复制到 plugins/lib/
cmake --build build -j$(nproc)

# 服务器从 config.yaml 的 plugins.directory 加载（默认 ./plugins/lib/）
./build/solarmcpd --config config.yaml
```

## 热重载

修改插件源码并重新编译后，无需重启服务器：

```bash
cmake --build build --target shell_plugin
python3 scripts/reload_plugins.py --list-tools
```

详见 [docs/插件热重载.md](../docs/插件热重载.md)。

## 新增插件

1. 可参考 `example/minimal_plugin.cpp` 复制起步
2. 在 `plugins/CMakeLists.txt` 中 `add_library(... MODULE ...)`
3. 将 POST_BUILD 复制目标加入 `lib/` 目录
4. 重新编译，调用 `plugins/reload` 或重启服务即可加载

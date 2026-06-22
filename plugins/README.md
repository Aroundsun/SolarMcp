# 插件目录说明

本目录采用 **「一插件一子目录」**：每个插件子目录包含 `.so` 与同目录下的独立配置文件。

## 目录结构

```
plugins/
├── CMakeLists.txt
├── README.md
├── example/                    # 最小示例插件
│   ├── minimal_plugin.c
│   ├── minimal_plugin.so       # 构建后生成（gitignore）
│   └── minimal.yaml            # 插件自有配置（可选）
└── shell/                      # shell 工具插件
    ├── shell_tool.h / .cpp
    ├── shell_plugin.cpp
    ├── shell_plugin.so         # 构建后生成（gitignore）
    └── shell.yaml                # 插件自有配置
```

> `read_file` 为**内置工具**（见 `config.yaml` 的 `tools.read_file`），不是插件。

## 配置原则

- **Core**（`config.yaml`）：仅框架 + 内置工具 + `plugins.directory`
- **Plugin**（`plugins/{name}/*.yaml`）：各插件业务配置，Core 只发现路径、不解析内容

详见 [docs/配置管理.md](../docs/配置管理.md)。

## 构建与加载

```bash
cmake --build build -j$(nproc)

# config.yaml: plugins.directory: "./plugins/"
./build/solarmcpd --config config.yaml
```

Core 扫描 `plugins/` 下各**子目录**中的 `.so`，并按以下顺序查找配置文件：

1. `plugin.yaml`
2. `{子目录名}.yaml`（如 `shell/shell.yaml`）
3. `{so 文件名}.yaml`（如 `shell_plugin.yaml`）

## 热重载

```bash
cmake --build build --target shell_plugin
python3 scripts/reload_plugins.py --list-tools
```

修改 `shell.yaml` 后热重载即可生效，**无需**改 `config.yaml`。

## 新增插件

1. 创建 `plugins/myplugin/`，放入源码与 `myplugin.yaml`
2. 在 `plugins/CMakeLists.txt` 添加 `add_library(... MODULE ...)`
3. POST_BUILD 将 `.so` 复制到 `plugins/myplugin/`
4. 实现 `plugin_abi.h` 六个导出符号，通过 `get_plugin_config_path()` 读取配置

**不要**链接 `libsolarmcp.a` 或在主配置 `config.yaml` 中添加 `tools.myplugin.*`。

# 插件目录说明

本目录存放插件**源码**与**构建配置**；编译生成的动态库（`.so`）输出到 `lib/` 子目录。

## 目录结构

```
plugins/
├── CMakeLists.txt          # 插件构建规则
├── README.md               # 本文件
├── filesystem/             # read_file 插件源码
│   └── read_file_plugin.cpp
├── shell/                  # shell 工具插件源码
│   ├── shell_tool.h
│   ├── shell_tool.cpp
│   └── shell_plugin.cpp
└── lib/                    # 运行时加载的 .so（构建后生成，已 gitignore）
    ├── read_file_plugin.so
    └── shell_plugin.so
```

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

1. 在 `plugins/` 下新建子目录，实现 `Tool` 与 `mcp_plugin_register` C ABI
2. 在 `plugins/CMakeLists.txt` 中 `add_library(... MODULE ...)`
3. 将 POST_BUILD 复制目标加入 `lib/` 目录
4. 重新编译，调用 `plugins/reload` 或重启服务即可加载

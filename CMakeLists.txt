cmake_minimum_required(VERSION 3.20)
project(SolarMcp VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# C++20 标准
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# 编译选项
# ---------------------------------------------------------------------------
if(NOT MSVC)
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# ---------------------------------------------------------------------------
# 第三方依赖（优先系统包，回退 FetchContent）
# ---------------------------------------------------------------------------
include(third_party/deps.cmake)

# ---------------------------------------------------------------------------
# SolarMcp 库
# ---------------------------------------------------------------------------
set(MCP_SOURCES
    # 日志
    src/logger/logger.cpp
    src/logger/async_logger.cpp
    # 配置
    src/config/config_manager.cpp
    # 线程
    src/thread/thread_pool.cpp
    # 网络
    src/network/buffer.cpp
    src/network/socket.cpp
    src/network/inet_address.cpp
    # Reactor
    src/reactor/event_loop.cpp
    src/reactor/channel.cpp
    src/reactor/poller.cpp
    src/reactor/epoll_poller.cpp
    # 协议
    src/protocol/json_rpc_codec.cpp
    src/protocol/request.cpp
    src/protocol/response.cpp
    # 服务器
    src/server/tcp_server.cpp
    src/server/tcp_connection.cpp
    src/server/dispatcher.cpp
    # 工具
    src/tool/tool_manager.cpp
    src/tool/read_file_tool.cpp
    # 插件
    src/plugin/plugin_manager.cpp
    # 定时器
    src/timer/timer_wheel.cpp
)

add_library(solarmcp STATIC ${MCP_SOURCES})
target_include_directories(solarmcp PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(solarmcp PUBLIC
    nlohmann_json::nlohmann_json
    yaml-cpp::yaml-cpp
    fmt::fmt
    pthread
    ${CMAKE_DL_LIBS}
)

# ---------------------------------------------------------------------------
# 主可执行文件
# ---------------------------------------------------------------------------
add_executable(solarmcpd app/main.cpp)
target_include_directories(solarmcpd PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(solarmcpd PRIVATE solarmcp)
# 向插件导出主程序符号（ToolManager、ReadFileTool 等）
if(NOT MSVC)
    target_link_options(solarmcpd PRIVATE "-rdynamic")
endif()

# ---------------------------------------------------------------------------
# 插件（可选）
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_SOURCE_DIR}/plugins/CMakeLists.txt)
    add_subdirectory(plugins)
endif()

# ---------------------------------------------------------------------------
# 测试（可选）
# ---------------------------------------------------------------------------
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

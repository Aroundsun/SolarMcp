#include "mcp/common/macros.h"
#include "mcp/config/config_manager.h"
#include "mcp/logger/logger.h"
#include "mcp/thread/thread_pool.h"
#include "mcp/plugin/plugin_manager.h"
#include "mcp/tool/tool_manager.h"
#include "mcp/tool/read_file_tool.h"
#include "mcp/server/dispatcher.h"
#include "mcp/server/tcp_server.h"
#include "mcp/reactor/event_loop.h"
#include "mcp/network/inet_address.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

/// 优雅关闭的全局原子标志
std::atomic<bool> g_running{true};

/// SIGINT / SIGTERM 信号处理函数
void signalHandler(int /*sig*/) {
    g_running.store(false, std::memory_order_release);
}

/// 解析命令行参数 --config <path>
std::string parseConfigPath(int argc, char* argv[]) {
    std::string config_path = "config.yaml";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            config_path = argv[++i];
        }
    }
    return config_path;
}

/// 从字符串解析 LogLevel。
mcp::LogLevel parseLogLevel(const std::string& level_str) {
    if (level_str == "trace") return mcp::LogLevel::TRACE;
    if (level_str == "debug") return mcp::LogLevel::DEBUG;
    if (level_str == "warn")  return mcp::LogLevel::WARN;
    if (level_str == "error") return mcp::LogLevel::ERROR;
    if (level_str == "fatal") return mcp::LogLevel::FATAL;
    return mcp::LogLevel::INFO;
}

/// 在 Dispatcher 上注册 MCP 内置方法（tools/list、tools/call）。
void registerMcpMethods(mcp::Dispatcher& dispatcher,
                         mcp::ToolManager& tool_manager) {
    // tools/list — 返回所有已注册工具的元数据
    dispatcher.registerMethod("tools/list",
        [&tool_manager](const nlohmann::json& /*params*/) -> nlohmann::json {
            auto tools = tool_manager.listTools();

            nlohmann::json tools_array = nlohmann::json::array();
            for (const auto& t : tools) {
                tools_array.push_back({
                    {"name", t.name},
                    {"description", t.description},
                    {"inputSchema", t.input_schema}
                });
            }

            return {
                {"tools", std::move(tools_array)}
            };
        });

    // tools/call — 按名称调用工具
    dispatcher.registerMethod("tools/call",
        [&tool_manager](const nlohmann::json& params) -> nlohmann::json {
            if (!params.contains("name") || !params["name"].is_string()) {
                throw std::runtime_error("Missing required parameter: 'name'");
            }

            std::string tool_name = params["name"].get<std::string>();
            nlohmann::json arguments;
            if (params.contains("arguments")) {
                arguments = params["arguments"];
            }

            mcp::Context ctx;
            auto result = tool_manager.callTool(tool_name, arguments, ctx);

            if (result.is_error) {
                throw std::runtime_error(result.error->message);
            }

            return result.content;
        });
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// main — SolarMcp 完整服务器入口
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // --- 1. 解析命令行 ---
    std::string config_path = parseConfigPath(argc, argv);

    // --- 2. 加载配置 ---
    auto& config = mcp::ConfigManager::getInstance();
    if (!config.loadFile(config_path)) {
        std::cerr << "Failed to load config: " << config_path << std::endl;
        return EXIT_FAILURE;
    }

    // --- 3. 初始化日志 ---
    std::string log_file = config.getString("logging.file", "./logs/solarmcp.log");
    mcp::LogLevel log_level = parseLogLevel(
        config.getString("logging.level", "info"));

    mcp::Logger::getInstance().init(log_file, log_level);

    LOG_INFO("============================================");
    LOG_INFO("  SolarMcp Server v0.1.0");
    LOG_INFO("  MCP (Model Context Protocol) C++20 Framework");
    LOG_INFO("============================================");
    LOG_INFO("Config file: {}", config_path);

    // --- 4. 创建线程池 ---
    int worker_threads = config.getInt("thread_pool.worker_threads", 4);
    auto thread_pool = std::make_shared<mcp::ThreadPool>(
        static_cast<size_t>(worker_threads));
    LOG_INFO("Thread pool: {} workers", worker_threads);

    // PluginManager 须先于 ToolManager 声明：析构时先销毁 Tool，再 dlclose。
    mcp::PluginManager plugin_manager;
    mcp::ToolManager tool_manager;

    // --- 6. 注册内置工具 ---
    if (config.getBool("tools.read_file.enabled", true)) {
        size_t max_size_mb = static_cast<size_t>(
            config.getInt("tools.read_file.max_size_mb", 10));
        size_t max_size_bytes = max_size_mb * 1024 * 1024;

        auto allowed_paths = config.getStringArray(
            "tools.read_file.allowed_paths");
        if (allowed_paths.empty()) {
            allowed_paths = {"/tmp", "/home"};
        }

        auto read_file_tool = std::make_unique<mcp::ReadFileTool>(
            max_size_bytes, std::move(allowed_paths));

        if (tool_manager.registerTool(std::move(read_file_tool))) {
            LOG_INFO("Registered built-in tool: read_file");
        }
    }

    // --- 7. 加载插件 ---
    std::string plugin_dir = config.getString("plugins.directory", "./plugins/");
    bool autoload = config.getBool("plugins.autoload", true);

    if (autoload) {
        int plugin_count = plugin_manager.loadFromDirectory(
            plugin_dir, tool_manager, config_path);
        if (plugin_count > 0) {
            LOG_INFO("Loaded {} plugin(s) from {}", plugin_count, plugin_dir);
        } else {
            LOG_DEBUG("No plugins loaded from {}", plugin_dir);
        }
    }

    LOG_INFO("Total tools registered: {}", tool_manager.size());

    // --- 8. 设置分发器 ---
    mcp::Dispatcher dispatcher;
    registerMcpMethods(dispatcher, tool_manager);

    LOG_INFO("Dispatcher: {} method(s) registered", dispatcher.methodCount());

    // --- 9. 创建事件循环与 TCP 服务器 ---
    mcp::EventLoop event_loop;

    std::string host = config.getString("server.host", "0.0.0.0");
    int port = config.getInt("server.port", 8090);

    mcp::InetAddress listen_addr(static_cast<uint16_t>(port), host);
    mcp::TcpServer server(&event_loop, listen_addr);

    server.setDispatcher(&dispatcher);
    server.setToolManager(&tool_manager);
    server.setThreadPool(thread_pool);

    // ---- 认证配置 ----
    if (config.getBool("auth.enabled", false)) {
        std::string auth_token = config.getString("auth.token", "");
        if (!auth_token.empty()) {
            server.setAuthToken(auth_token);
            LOG_INFO("Token authentication enabled");
        } else {
            LOG_WARN("auth.enabled=true but auth.token is empty — "
                     "authentication disabled");
        }
    }

    server.start();

    // --- 10. 注册信号处理 ---
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    LOG_INFO("Server listening on {}:{}", host, port);
    LOG_INFO("Press Ctrl+C to stop");

    // --- 11. 主事件循环 ---
    // 以后台「看门狗」模式运行 loop：
    // EventLoop 在当前线程运行，信号监控线程
    // 定期检查 g_running 并调用 quit()。
    std::thread signal_monitor([&event_loop]() {
        while (g_running.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        event_loop.quit();
    });

    event_loop.loop();
    signal_monitor.join();

    // --- 12. 优雅关闭 ---
    LOG_INFO("Shutting down...");
    server.stop();
    tool_manager.clear();
    plugin_manager.unloadAll();
    thread_pool->shutdown();
    mcp::Logger::getInstance().shutdown();

    std::cerr << "[SolarMcp] Server stopped." << std::endl;
    return EXIT_SUCCESS;
}

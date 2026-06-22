#include "mcp/plugin/plugin_manager.h"
#include "mcp/tool/read_file_tool.h"
#include "mcp/tool/tool_manager.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

namespace fs = std::filesystem;

#ifndef SOLARMCP_PLUGIN_DIR
#define SOLARMCP_PLUGIN_DIR "plugins"
#endif

std::string pluginDir() {
    return SOLARMCP_PLUGIN_DIR;
}

bool shellPluginAvailable() {
    return fs::exists(fs::path(pluginDir()) / "shell_plugin.so");
}

bool minimalPluginAvailable() {
    return fs::exists(fs::path(pluginDir()) / "minimal_plugin.so");
}

bool allPluginsAvailable() {
    return shellPluginAvailable() && minimalPluginAvailable();
}

struct PluginFixture {
    mcp::PluginManager pm;
    mcp::ToolManager tm;

    ~PluginFixture() { shutdown(); }

    void shutdown() {
        pm.unloadAll(tm);
    }
};

TEST(PluginManagerTest, LoadFromEmptyDirectory) {
    PluginFixture fx;

    int count = fx.pm.loadFromDirectory("/tmp", fx.tm);
    EXPECT_GE(count, 0);

    fx.pm.unloadAll(fx.tm);
    EXPECT_EQ(fx.pm.loadedCount(), 0u);
}

TEST(PluginManagerTest, LoadFromNonexistentDirectory) {
    PluginFixture fx;

    int count = fx.pm.loadFromDirectory("/tmp/nonexistent_dir_abc123", fx.tm);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(fx.pm.loadedCount(), 0u);
}

TEST(PluginManagerTest, UnloadAllOnEmptyManager) {
    PluginFixture fx;
    fx.pm.unloadAll(fx.tm);
    EXPECT_EQ(fx.pm.loadedCount(), 0u);
}

TEST(PluginManagerTest, LoadedCountInitial) {
    mcp::PluginManager pm;
    EXPECT_EQ(pm.loadedCount(), 0u);
}

TEST(PluginManagerTest, IgnoresInvalidSharedLibrary) {
    const fs::path dir =
        fs::temp_directory_path() / "solarmcp_plugin_invalid_test";
    fs::create_directories(dir);

    {
        std::ofstream out(dir / "invalid_plugin.so");
        out << "not an ELF shared object";
    }

    PluginFixture fx;
    int count = fx.pm.loadFromDirectory(dir.string(), fx.tm);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(fx.pm.loadedCount(), 0u);

    fs::remove_all(dir);
}

TEST(PluginManagerTest, LoadShellPlugin) {
    if (!shellPluginAvailable()) {
        GTEST_SKIP() << "shell_plugin.so not found in " << pluginDir();
    }

    PluginFixture fx;
    const std::string shell_so = (fs::path(pluginDir()) / "shell_plugin.so").string();

    const fs::path dir =
        fs::temp_directory_path() / "solarmcp_plugin_shell_only";
    fs::create_directories(dir);
    fs::copy_file(shell_so, dir / "shell_plugin.so",
                  fs::copy_options::overwrite_existing);

    int count = fx.pm.loadFromDirectory(dir.string(), fx.tm, "");
    EXPECT_EQ(count, 1);
    EXPECT_EQ(fx.pm.loadedCount(), 1u);

    ASSERT_NE(fx.tm.getTool("shell"), nullptr);

    mcp::Context ctx;
    auto result = fx.tm.callTool("shell",
                                 {{"command", "echo plugin_ok"}}, ctx);
    ASSERT_FALSE(result.is_error);
    EXPECT_EQ(result.content["status"], "success");
    EXPECT_NE(result.content["stdout"].get<std::string>().find("plugin_ok"),
              std::string::npos);

    fs::remove_all(dir);
}

TEST(PluginManagerTest, LoadMinimalPlugin) {
    if (!minimalPluginAvailable()) {
        GTEST_SKIP() << "minimal_plugin.so not found in " << pluginDir();
    }

    PluginFixture fx;
    const std::string so = (fs::path(pluginDir()) / "minimal_plugin.so").string();

    const fs::path dir =
        fs::temp_directory_path() / "solarmcp_plugin_minimal_only";
    fs::create_directories(dir);
    fs::copy_file(so, dir / "minimal_plugin.so",
                  fs::copy_options::overwrite_existing);

    EXPECT_EQ(fx.pm.loadFromDirectory(dir.string(), fx.tm, ""), 1);
    ASSERT_NE(fx.tm.getTool("plugin_demo"), nullptr);

    mcp::Context ctx;
    auto result = fx.tm.callTool("plugin_demo", {}, ctx);
    ASSERT_FALSE(result.is_error);
    EXPECT_EQ(result.content["message"], "hello from minimal plugin");

    fs::remove_all(dir);
}

TEST(PluginManagerTest, LoadAllPluginsFromDirectory) {
    if (!allPluginsAvailable()) {
        GTEST_SKIP() << "plugin .so files not found in " << pluginDir();
    }

    PluginFixture fx;
    int count = fx.pm.loadFromDirectory(pluginDir(), fx.tm, "");
    EXPECT_EQ(count, 2);
    EXPECT_EQ(fx.pm.loadedCount(), 2u);
    EXPECT_GE(fx.tm.size(), 2u);
    EXPECT_NE(fx.tm.getTool("shell"), nullptr);
    EXPECT_NE(fx.tm.getTool("plugin_demo"), nullptr);
}

TEST(PluginManagerTest, MinimalPluginAlongsideBuiltinReadFile) {
    if (!allPluginsAvailable()) {
        GTEST_SKIP() << "plugin .so files not found in " << pluginDir();
    }

    PluginFixture fx;
    ASSERT_TRUE(fx.tm.registerTool(std::make_unique<mcp::ReadFileTool>()));

    int count = fx.pm.loadFromDirectory(pluginDir(), fx.tm, "");
    EXPECT_EQ(count, 2);
    EXPECT_NE(fx.tm.getTool("shell"), nullptr);
    EXPECT_NE(fx.tm.getTool("plugin_demo"), nullptr);
    EXPECT_NE(fx.tm.getTool("read_file"), nullptr);
    EXPECT_EQ(fx.tm.size(), 3u);
}

TEST(PluginManagerTest, ShellDisabledViaConfig) {
    if (!shellPluginAvailable()) {
        GTEST_SKIP() << "shell_plugin.so not found in " << pluginDir();
    }

    const fs::path cfg =
        fs::temp_directory_path() / "solarmcp_shell_disabled.yaml";
    {
        std::ofstream out(cfg);
        out << "tools:\n  shell:\n    enabled: false\n";
    }

    const fs::path dir =
        fs::temp_directory_path() / "solarmcp_plugin_shell_cfg";
    fs::create_directories(dir);
    fs::copy_file(fs::path(pluginDir()) / "shell_plugin.so",
                  dir / "shell_plugin.so",
                  fs::copy_options::overwrite_existing);

    PluginFixture fx;
    int count = fx.pm.loadFromDirectory(dir.string(), fx.tm, cfg.string());
    EXPECT_EQ(count, 1);
    EXPECT_EQ(fx.pm.loadedCount(), 1u);
    EXPECT_EQ(fx.tm.getTool("shell"), nullptr);

    fs::remove(cfg);
    fs::remove_all(dir);
}

TEST(PluginManagerTest, ShellPluginReadsConfigPath) {
    if (!shellPluginAvailable()) {
        GTEST_SKIP() << "shell_plugin.so not found in " << pluginDir();
    }

    const fs::path cfg =
        fs::temp_directory_path() / "solarmcp_shell_timeout.yaml";
    {
        std::ofstream out(cfg);
        out << "tools:\n  shell:\n    enabled: true\n    timeout_sec: 15\n";
    }

    const fs::path dir =
        fs::temp_directory_path() / "solarmcp_plugin_shell_timeout";
    fs::create_directories(dir);
    fs::copy_file(fs::path(pluginDir()) / "shell_plugin.so",
                  dir / "shell_plugin.so",
                  fs::copy_options::overwrite_existing);

    PluginFixture fx;
    ASSERT_EQ(fx.pm.loadFromDirectory(dir.string(), fx.tm, cfg.string()), 1);

    auto* shell = fx.tm.getTool("shell");
    ASSERT_NE(shell, nullptr);
    auto schema = shell->inputSchema();
    const std::string desc =
        schema["properties"]["timeout_seconds"]["description"].get<std::string>();
    EXPECT_NE(desc.find("15"), std::string::npos);

    fs::remove(cfg);
    fs::remove_all(dir);
}

TEST(PluginManagerTest, SafeShutdownAfterLoad) {
    if (!shellPluginAvailable()) {
        GTEST_SKIP() << "shell_plugin.so not found in " << pluginDir();
    }

    const fs::path dir =
        fs::temp_directory_path() / "solarmcp_plugin_shutdown";
    fs::create_directories(dir);
    fs::copy_file(fs::path(pluginDir()) / "shell_plugin.so",
                  dir / "shell_plugin.so",
                  fs::copy_options::overwrite_existing);

    {
        PluginFixture fx;
        ASSERT_EQ(fx.pm.loadFromDirectory(dir.string(), fx.tm, ""), 1);
        ASSERT_NE(fx.tm.getTool("shell"), nullptr);
        fx.shutdown();
    }

    fs::remove_all(dir);
}

TEST(PluginManagerTest, ReloadFromDirectory) {
    if (!shellPluginAvailable()) {
        GTEST_SKIP() << "shell_plugin.so not found in " << pluginDir();
    }

    const fs::path dir =
        fs::temp_directory_path() / "solarmcp_plugin_reload";
    fs::create_directories(dir);
    fs::copy_file(fs::path(pluginDir()) / "shell_plugin.so",
                  dir / "shell_plugin.so",
                  fs::copy_options::overwrite_existing);

    PluginFixture fx;
    ASSERT_TRUE(fx.tm.registerTool(std::make_unique<mcp::ReadFileTool>()));
    ASSERT_EQ(fx.pm.loadFromDirectory(dir.string(), fx.tm, ""), 1);
    ASSERT_NE(fx.tm.getTool("shell"), nullptr);
    EXPECT_EQ(fx.tm.size(), 2u);

    auto result = fx.pm.reloadFromDirectory(dir.string(), fx.tm, "");
    EXPECT_EQ(result.unloaded, 1);
    EXPECT_EQ(result.loaded, 1);
    EXPECT_EQ(fx.pm.loadedCount(), 1u);
    EXPECT_NE(fx.tm.getTool("shell"), nullptr);
    EXPECT_NE(fx.tm.getTool("read_file"), nullptr);
    EXPECT_EQ(fx.tm.size(), 2u);

    mcp::Context ctx;
    auto call = fx.tm.callTool("shell", {{"command", "echo reloaded"}}, ctx);
    ASSERT_FALSE(call.is_error);
    EXPECT_NE(call.content["stdout"].get<std::string>().find("reloaded"),
              std::string::npos);

    fs::remove_all(dir);
}

TEST(PluginManagerTest, ReloadSinglePlugin) {
    if (!shellPluginAvailable()) {
        GTEST_SKIP() << "shell_plugin.so not found in " << pluginDir();
    }

    const fs::path dir =
        fs::temp_directory_path() / "solarmcp_plugin_single_reload";
    fs::create_directories(dir);
    const std::string shell_so = (dir / "shell_plugin.so").string();
    fs::copy_file(fs::path(pluginDir()) / "shell_plugin.so", shell_so,
                  fs::copy_options::overwrite_existing);

    PluginFixture fx;
    ASSERT_TRUE(fx.pm.reloadPlugin(shell_so, fx.tm, ""));
    ASSERT_NE(fx.tm.getTool("shell"), nullptr);

    ASSERT_TRUE(fx.pm.reloadPlugin(shell_so, fx.tm, ""));
    ASSERT_NE(fx.tm.getTool("shell"), nullptr);
    EXPECT_EQ(fx.pm.loadedCount(), 1u);

    fs::remove_all(dir);
}

TEST(PluginManagerTest, UnregisterToolOnly) {
    PluginFixture fx;
    ASSERT_TRUE(fx.tm.registerTool(std::make_unique<mcp::ReadFileTool>()));
    EXPECT_TRUE(fx.tm.unregisterTool("read_file"));
    EXPECT_FALSE(fx.tm.unregisterTool("read_file"));
    EXPECT_EQ(fx.tm.size(), 0u);
}

} // anonymous namespace

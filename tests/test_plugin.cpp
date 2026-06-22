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

fs::path pluginsRoot() {
    return SOLARMCP_PLUGIN_DIR;
}

fs::path shellSoPath() {
    return pluginsRoot() / "shell" / "shell_plugin.so";
}

fs::path minimalSoPath() {
    return pluginsRoot() / "example" / "minimal_plugin.so";
}

fs::path shellYamlPath() {
    return pluginsRoot() / "shell" / "shell.yaml";
}

bool shellPluginAvailable() {
    return fs::exists(shellSoPath());
}

bool minimalPluginAvailable() {
    return fs::exists(minimalSoPath());
}

bool allPluginsAvailable() {
    return shellPluginAvailable() && minimalPluginAvailable();
}

void copyShellBundle(const fs::path& dest_root,
                     const std::string& yaml_content = "") {
    const fs::path shell_dir = dest_root / "shell";
    fs::create_directories(shell_dir);
    fs::copy_file(shellSoPath(), shell_dir / "shell_plugin.so",
                  fs::copy_options::overwrite_existing);
    if (!yaml_content.empty()) {
        std::ofstream out(shell_dir / "shell.yaml");
        out << yaml_content;
    } else if (fs::exists(shellYamlPath())) {
        fs::copy_file(shellYamlPath(), shell_dir / "shell.yaml",
                      fs::copy_options::overwrite_existing);
    }
}

void copyMinimalBundle(const fs::path& dest_root) {
    const fs::path example_dir = dest_root / "example";
    fs::create_directories(example_dir);
    fs::copy_file(minimalSoPath(), example_dir / "minimal_plugin.so",
                  fs::copy_options::overwrite_existing);
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
    const fs::path root =
        fs::temp_directory_path() / "solarmcp_plugin_invalid_test";
    const fs::path bad_dir = root / "badplugin";
    fs::create_directories(bad_dir);

    {
        std::ofstream out(bad_dir / "invalid_plugin.so");
        out << "not an ELF shared object";
    }

    PluginFixture fx;
    int count = fx.pm.loadFromDirectory(root.string(), fx.tm);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(fx.pm.loadedCount(), 0u);

    fs::remove_all(root);
}

TEST(PluginManagerTest, LoadShellPlugin) {
    if (!shellPluginAvailable()) {
        GTEST_SKIP() << "shell_plugin.so not found under plugins/shell/";
    }

    PluginFixture fx;
    const fs::path root =
        fs::temp_directory_path() / "solarmcp_plugin_shell_only";
    copyShellBundle(root);

    int count = fx.pm.loadFromDirectory(root.string(), fx.tm);
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

    fs::remove_all(root);
}

TEST(PluginManagerTest, LoadMinimalPlugin) {
    if (!minimalPluginAvailable()) {
        GTEST_SKIP() << "minimal_plugin.so not found under plugins/example/";
    }

    PluginFixture fx;
    const fs::path root =
        fs::temp_directory_path() / "solarmcp_plugin_minimal_only";
    copyMinimalBundle(root);

    EXPECT_EQ(fx.pm.loadFromDirectory(root.string(), fx.tm), 1);
    ASSERT_NE(fx.tm.getTool("plugin_demo"), nullptr);

    mcp::Context ctx;
    auto result = fx.tm.callTool("plugin_demo", {}, ctx);
    ASSERT_FALSE(result.is_error);
    EXPECT_EQ(result.content["message"], "hello from minimal plugin");

    fs::remove_all(root);
}

TEST(PluginManagerTest, LoadAllPluginsFromDirectory) {
    if (!allPluginsAvailable()) {
        GTEST_SKIP() << "plugin bundles not found under plugins/";
    }

    PluginFixture fx;
    int count = fx.pm.loadFromDirectory(pluginsRoot().string(), fx.tm);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(fx.pm.loadedCount(), 2u);
    EXPECT_GE(fx.tm.size(), 2u);
    EXPECT_NE(fx.tm.getTool("shell"), nullptr);
    EXPECT_NE(fx.tm.getTool("plugin_demo"), nullptr);
}

TEST(PluginManagerTest, MinimalPluginAlongsideBuiltinReadFile) {
    if (!allPluginsAvailable()) {
        GTEST_SKIP() << "plugin bundles not found under plugins/";
    }

    PluginFixture fx;
    ASSERT_TRUE(fx.tm.registerTool(std::make_unique<mcp::ReadFileTool>()));

    int count = fx.pm.loadFromDirectory(pluginsRoot().string(), fx.tm);
    EXPECT_EQ(count, 2);
    EXPECT_NE(fx.tm.getTool("shell"), nullptr);
    EXPECT_NE(fx.tm.getTool("plugin_demo"), nullptr);
    EXPECT_NE(fx.tm.getTool("read_file"), nullptr);
    EXPECT_EQ(fx.tm.size(), 3u);
}

TEST(PluginManagerTest, ShellDisabledViaPluginConfig) {
    if (!shellPluginAvailable()) {
        GTEST_SKIP() << "shell_plugin.so not found under plugins/shell/";
    }

    const fs::path root =
        fs::temp_directory_path() / "solarmcp_plugin_shell_cfg";
    copyShellBundle(root, "enabled: false\n");

    PluginFixture fx;
    int count = fx.pm.loadFromDirectory(root.string(), fx.tm);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(fx.pm.loadedCount(), 1u);
    EXPECT_EQ(fx.tm.getTool("shell"), nullptr);

    fs::remove_all(root);
}

TEST(PluginManagerTest, ShellPluginReadsPluginYaml) {
    if (!shellPluginAvailable()) {
        GTEST_SKIP() << "shell_plugin.so not found under plugins/shell/";
    }

    const fs::path root =
        fs::temp_directory_path() / "solarmcp_plugin_shell_timeout";
    copyShellBundle(root,
        "enabled: true\n"
        "timeout_sec: 15\n"
        "max_output_mb: 10\n"
        "allowed_shells: [\"/bin/sh\"]\n");

    PluginFixture fx;
    ASSERT_EQ(fx.pm.loadFromDirectory(root.string(), fx.tm), 1);

    auto* shell = fx.tm.getTool("shell");
    ASSERT_NE(shell, nullptr);
    auto schema = shell->inputSchema();
    const std::string desc =
        schema["properties"]["timeout_seconds"]["description"].get<std::string>();
    EXPECT_NE(desc.find("15"), std::string::npos);

    fs::remove_all(root);
}

TEST(PluginManagerTest, SafeShutdownAfterLoad) {
    if (!shellPluginAvailable()) {
        GTEST_SKIP() << "shell_plugin.so not found under plugins/shell/";
    }

    const fs::path root =
        fs::temp_directory_path() / "solarmcp_plugin_shutdown";
    copyShellBundle(root);

    {
        PluginFixture fx;
        ASSERT_EQ(fx.pm.loadFromDirectory(root.string(), fx.tm), 1);
        ASSERT_NE(fx.tm.getTool("shell"), nullptr);
        fx.shutdown();
    }

    fs::remove_all(root);
}

TEST(PluginManagerTest, ReloadFromDirectory) {
    if (!shellPluginAvailable()) {
        GTEST_SKIP() << "shell_plugin.so not found under plugins/shell/";
    }

    const fs::path root =
        fs::temp_directory_path() / "solarmcp_plugin_reload";
    copyShellBundle(root);

    PluginFixture fx;
    ASSERT_TRUE(fx.tm.registerTool(std::make_unique<mcp::ReadFileTool>()));
    ASSERT_EQ(fx.pm.loadFromDirectory(root.string(), fx.tm), 1);
    ASSERT_NE(fx.tm.getTool("shell"), nullptr);
    EXPECT_EQ(fx.tm.size(), 2u);

    auto result = fx.pm.reloadFromDirectory(root.string(), fx.tm);
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

    fs::remove_all(root);
}

TEST(PluginManagerTest, ReloadSinglePlugin) {
    if (!shellPluginAvailable()) {
        GTEST_SKIP() << "shell_plugin.so not found under plugins/shell/";
    }

    const fs::path root =
        fs::temp_directory_path() / "solarmcp_plugin_single_reload";
    copyShellBundle(root);
    const std::string shell_so = (root / "shell" / "shell_plugin.so").string();

    PluginFixture fx;
    ASSERT_TRUE(fx.pm.reloadPlugin(shell_so, fx.tm));
    ASSERT_NE(fx.tm.getTool("shell"), nullptr);

    ASSERT_TRUE(fx.pm.reloadPlugin(shell_so, fx.tm));
    ASSERT_NE(fx.tm.getTool("shell"), nullptr);
    EXPECT_EQ(fx.pm.loadedCount(), 1u);

    fs::remove_all(root);
}

TEST(PluginManagerTest, UnregisterToolOnly) {
    PluginFixture fx;
    ASSERT_TRUE(fx.tm.registerTool(std::make_unique<mcp::ReadFileTool>()));
    EXPECT_TRUE(fx.tm.unregisterTool("read_file"));
    EXPECT_FALSE(fx.tm.unregisterTool("read_file"));
    EXPECT_EQ(fx.tm.size(), 0u);
}

} // anonymous namespace
